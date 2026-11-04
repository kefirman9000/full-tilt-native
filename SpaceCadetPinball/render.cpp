#include "pch.h"
#include "render.h"
#include "side_panel.h"
#include "table_art.h"
#include "TTextBox.h"

#include "fullscrn.h"
#include "GroupData.h"
#include "options.h"
#include "pb.h"
#include "score.h"
#include "TPinballTable.h"
#include "winmain.h"
#include "DebugOverlay.h"
#include "proj.h"

std::vector<render_sprite*> render::sprite_list, render::ball_list;
zmap_header_type* render::background_zmap;
int render::zmap_offsetX, render::zmap_offsetY, render::offset_x, render::offset_y;
rectangle_type render::vscreen_rect;
gdrv_bitmap8 *render::vscreen, *render::background_bitmap, *render::ball_bitmap[64];
zmap_header_type* render::zscreen;
SDL_Rect render::DestinationRect{};
SDL_Rect render::SidePanelRect{};

render_sprite::render_sprite(VisualTypes visualType, gdrv_bitmap8* bmp, zmap_header_type* zMap,
	int xPosition, int yPosition, rectangle_type* boundingRect)
{
	Bmp = bmp;
	ZMap = zMap;
	VisualType = visualType;
	DeleteFlag = false;
	OccludedSprites = nullptr;
	DirtyRect = rectangle_type{};
	DirtyFlag = visualType != VisualTypes::Ball;
	ZMapOffestX = 0;
	ZMapOffestY = 0;
	Depth = 0xffFF;

	if (boundingRect)
	{
		BoundingRect = *boundingRect;
	}
	else
	{
		BoundingRect.Width = -1;
		BoundingRect.Height = -1;
		BoundingRect.XPosition = 0;
		BoundingRect.YPosition = 0;
	}

	BmpRect.XPosition = xPosition;
	BmpRect.YPosition = yPosition;
	if (bmp)
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
	else
	{
		BmpRect.Width = 0;
		BmpRect.Height = 0;
	}
	DirtyRectPrev = BmpRect;

	if (!ZMap && VisualType != VisualTypes::Ball)
	{
		assertm(false, "Background zMap should not be used");
		ZMap = render::background_zmap;
		ZMapOffestY = xPosition - render::zmap_offsetX;
		ZMapOffestX = yPosition - render::zmap_offsetY;
	}

	render::AddSprite(*this);
}

render_sprite::~render_sprite()
{
	render::RemoveSprite(*this);
	delete OccludedSprites;
}

void render_sprite::set(gdrv_bitmap8* bmp, zmap_header_type* zMap, int xPos, int yPos)
{
	if (Bmp == bmp && ZMap == zMap && BmpRect.XPosition == xPos && BmpRect.YPosition == yPos)
		return;

	if (AllowFade && bmp != Bmp && (IsFlipper ? options::Options.SmoothFlippers : options::Options.SmoothLights))
		FadePending = true;
	Bmp = bmp;
	ZMap = zMap;
	DirtyFlag = VisualType != VisualTypes::Ball;
	BmpRect.XPosition = xPos;
	BmpRect.YPosition = yPos;
	if (bmp) 
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
}

void render_sprite::ball_set(gdrv_bitmap8* bmp, float depth, int xPos, int yPos)
{
	set(bmp, ZMap,xPos, yPos);
	Depth = proj::NormalizeDepth(depth);
	if (pb::DevNoZ) Depth = 0;
}

void render::init(gdrv_bitmap8* bmp, int width, int height)
{
	vscreen = new gdrv_bitmap8(width, height, false);
	zscreen = new zmap_header_type(width, height, width);
	zdrv::fill(zscreen, zscreen->Width, zscreen->Height, 0, 0, 0xFFFF);
	vscreen_rect.YPosition = 0;
	vscreen_rect.XPosition = 0;
	vscreen_rect.Width = width;
	vscreen_rect.Height = height;
	vscreen->YPosition = 0;
	vscreen->XPosition = 0;
	for (auto& ballBmp : ball_bitmap)
		ballBmp = new gdrv_bitmap8(256, 256, false); // balls and FT2 animations (Mad's bubbling is 60x129)

	background_bitmap = bmp;
	if (bmp)
		gdrv::copy_bitmap(vscreen, width, height, 0, 0, bmp, 0, 0);
	else
		gdrv::fill_bitmap(vscreen, vscreen->Width, vscreen->Height, 0, 0, 0);

	recreate_screen_texture();
}

void render::uninit()
{
	delete vscreen;
	delete zscreen;

	// Sprite destructor removes it from the list.
	while (!sprite_list.empty())
		delete sprite_list[0];
	while (!ball_list.empty())
		delete ball_list[0];
	for (auto& ballBmp : ball_bitmap)
		delete ballBmp;
	DebugOverlay::UnInit();
}

void render::recreate_screen_texture()
{
	vscreen->CreateTexture(options::Options.LinearFiltering ? "linear" : "nearest", SDL_TEXTUREACCESS_STREAMING);
}

void render::update()
{
	unpaint_balls();

	// Lights that just switched: remember how their area looked before the change
	for (auto sprite : sprite_list)
	{
		if (!sprite->FadePending)
			continue;
		sprite->FadePending = false;
		rectangle_type area{};
		if (sprite->DirtyRectPrev.Width > 0)
			maths::enclosing_box(sprite->DirtyRectPrev, sprite->BmpRect, area);
		else
			area = sprite->BmpRect;
		if (!maths::rectangle_clip(area, vscreen_rect, &area) || area.Width <= 0 || area.Height <= 0)
			continue;
		sprite->FadeRect = area;
		sprite->FadeFrom.resize(static_cast<size_t>(area.Width) * area.Height);
		for (int y = 0; y < area.Height; y++)
			memcpy(&sprite->FadeFrom[static_cast<size_t>(y) * area.Width],
			       &vscreen->BmpBufPtr1[(area.YPosition + y) * vscreen->Stride + area.XPosition],
			       sizeof(ColorRgba) * area.Width);
		sprite->FadeStart = SDL_GetTicks();
	}

	// Clip dirty sprites with vScreen, clear clipping (dirty) rectangles
	for (const auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		bool clearSprite = false;
		switch (sprite->VisualType)
		{
		case VisualTypes::Sprite:
			if (sprite->DirtyRectPrev.Width > 0)
				maths::enclosing_box(sprite->DirtyRectPrev, sprite->BmpRect, sprite->DirtyRect);

			if (maths::rectangle_clip(sprite->DirtyRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = true;
			else
				sprite->DirtyRect.Width = -1;
			break;
		case VisualTypes::Background:
			if (maths::rectangle_clip(sprite->BmpRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = !sprite->Bmp;
			else
				sprite->DirtyRect.Width = -1;
			break;
		default: break;
		}

		if (clearSprite)
		{
			auto yPos = sprite->DirtyRect.YPosition;
			auto width = sprite->DirtyRect.Width;
			auto xPos = sprite->DirtyRect.XPosition;
			auto height = sprite->DirtyRect.Height;
			zdrv::fill(zscreen, width, height, xPos, yPos, 0xFFFF);
			if (background_bitmap)
				gdrv::copy_bitmap(vscreen, width, height, xPos, yPos, background_bitmap, xPos, yPos);
			else
				gdrv::fill_bitmap(vscreen, width, height, xPos, yPos, 0);
		}
	}

	// Paint dirty rectangles of dirty sprites
	for (auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		repaint(*sprite);
		sprite->DirtyFlag = false;
		sprite->DirtyRectPrev = sprite->DirtyRect;
		if (sprite->DeleteFlag)
			delete sprite;
	}

	// Blend fading lights from their old look to the freshly painted one
	auto now = SDL_GetTicks();
	for (auto sprite : sprite_list)
	{
		if (sprite->FadeFrom.empty())
			continue;
		auto t = static_cast<float>(now - sprite->FadeStart) / sprite->FadeMs;
		if (t >= 1.0f || !(sprite->IsFlipper ? options::Options.SmoothFlippers : options::Options.SmoothLights))
		{
			sprite->FadeFrom.clear();
			sprite->DirtyFlag = true; // one last clean repaint at full brightness
			continue;
		}
		auto& area = sprite->FadeRect;
		auto mix = static_cast<uint32_t>(t * 256);
		for (int y = 0; y < area.Height; y++)
		{
			auto dst = &vscreen->BmpBufPtr1[(area.YPosition + y) * vscreen->Stride + area.XPosition];
			auto src = &sprite->FadeFrom[static_cast<size_t>(y) * area.Width];
			for (int x = 0; x < area.Width; x++)
			{
				auto a = src[x], b = dst[x];
				dst[x] = ColorRgba(
					static_cast<uint8_t>((a.GetRed() * (256 - mix) + b.GetRed() * mix) >> 8),
					static_cast<uint8_t>((a.GetGreen() * (256 - mix) + b.GetGreen() * mix) >> 8),
					static_cast<uint8_t>((a.GetBlue() * (256 - mix) + b.GetBlue() * mix) >> 8), 255);
			}
		}
		sprite->DirtyFlag = true; // repaint the new look next frame, then blend a little further
	}

	paint_balls();
}

void render::AddSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	list.push_back(&sprite);
}

void render::RemoveSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	auto it = std::find(list.begin(), list.end(), &sprite);
	if (it != list.end())
		list.erase(it);
}

void render::set_background_zmap(zmap_header_type* zMap, int offsetX, int offsetY)
{
	background_zmap = zMap;
	zmap_offsetX = offsetX;
	zmap_offsetY = offsetY;
}

void render::repaint(const render_sprite& sprite)
{
	rectangle_type clipRect{};
	if (!sprite.OccludedSprites || sprite.VisualType == VisualTypes::Ball || sprite.DirtyRect.Width <= 0)
		return;

	for (auto refSprite : *sprite.OccludedSprites)
	{
		if (!refSprite->DeleteFlag && refSprite->Bmp)
		{
			if (maths::rectangle_clip(refSprite->BmpRect, sprite.DirtyRect, &clipRect))
				zdrv::paint(
					clipRect.Width,
					clipRect.Height,
					vscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					zscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					refSprite->Bmp,
					clipRect.XPosition - refSprite->BmpRect.XPosition,
					clipRect.YPosition - refSprite->BmpRect.YPosition,
					refSprite->ZMap,
					clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
					clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition);
		}
	}
}


void render::restore_area(int x, int y, int width, int height)
{
	rectangle_type area{x, y, width, height}, clipRect{};
	if (!maths::rectangle_clip(area, vscreen_rect, &area) || area.Width <= 0 || area.Height <= 0)
		return;
	zdrv::fill(zscreen, area.Width, area.Height, area.XPosition, area.YPosition, 0xFFFF);
	if (background_bitmap)
		gdrv::copy_bitmap(vscreen, area.Width, area.Height, area.XPosition, area.YPosition, background_bitmap,
		                  area.XPosition, area.YPosition);
	else
		gdrv::fill_bitmap(vscreen, area.Width, area.Height, area.XPosition, area.YPosition, 0);
	for (auto sprite : sprite_list)
	{
		if (sprite->DeleteFlag || !sprite->Bmp || sprite->VisualType == VisualTypes::Ball)
			continue;
		if (maths::rectangle_clip(sprite->BmpRect, area, &clipRect))
			zdrv::paint(clipRect.Width, clipRect.Height, vscreen, clipRect.XPosition, clipRect.YPosition, zscreen,
			            clipRect.XPosition, clipRect.YPosition, sprite->Bmp,
			            clipRect.XPosition - sprite->BmpRect.XPosition, clipRect.YPosition - sprite->BmpRect.YPosition,
			            sprite->ZMap, clipRect.XPosition + sprite->ZMapOffestY - sprite->BmpRect.XPosition,
			            clipRect.YPosition + sprite->ZMapOffestX - sprite->BmpRect.YPosition);
	}
}

bool render::BoxesInSidePanel()
{
	return pb::FT2FullScreen && SidePanelRect.w > 0;
}

void render::paint_balls()
{
	// Sort ball sprites by ascending depth
	std::sort(ball_list.begin(), ball_list.end(), [](const render_sprite* lhs, const render_sprite* rhs)
	{
		return lhs->Depth < rhs->Depth;
	});

	// For balls that clip vScreen: save original vScreen contents and paint ball bitmap.
	for (auto index = 0u; index < ball_list.size(); ++index)
	{
		auto ball = ball_list[index];
		auto dirty = &ball->DirtyRect;
		if (ball->Bmp && maths::rectangle_clip(ball->BmpRect, vscreen_rect, &ball->DirtyRect))
		{
			int xPos = dirty->XPosition;
			int yPos = dirty->YPosition;
			gdrv::copy_bitmap(ball_bitmap[index], dirty->Width, dirty->Height, 0, 0, vscreen, xPos, yPos);
			zdrv::paint_flat(
				dirty->Width,
				dirty->Height,
				vscreen,
				xPos,
				yPos,
				zscreen,
				xPos,
				yPos,
				ball->Bmp,
				xPos - ball->BmpRect.XPosition,
				yPos - ball->BmpRect.YPosition,
				ball->Depth);
		}
		else
		{
			dirty->Width = -1;
		}
	}
}

void render::unpaint_balls()
{
	// Restore portions of vScreen saved during previous paint_balls call.
	for (int index = static_cast<int>(ball_list.size()) - 1; index >= 0; index--)
	{
		auto curBall = ball_list[index];
		if (curBall->DirtyRect.Width > 0)
			gdrv::copy_bitmap(
				vscreen,
				curBall->DirtyRect.Width,
				curBall->DirtyRect.Height,
				curBall->DirtyRect.XPosition,
				curBall->DirtyRect.YPosition,
				ball_bitmap[index],
				0,
				0);

		curBall->DirtyRectPrev = curBall->DirtyRect;
	}
}

void render::shift(int offsetX, int offsetY)
{
	offset_x += offsetX;
	offset_y += offsetY;
}

void render::build_occlude_list()
{
	std::vector<render_sprite*>* spriteArr = nullptr;
	for (auto mainSprite : sprite_list)
	{
		if (mainSprite->OccludedSprites)
		{
			delete mainSprite->OccludedSprites;
			mainSprite->OccludedSprites = nullptr;
		}

		if (!mainSprite->DeleteFlag && mainSprite->BoundingRect.Width != -1)
		{
			if (!spriteArr)
				spriteArr = new std::vector<render_sprite*>();

			for (auto refSprite : sprite_list)
			{
				if (!refSprite->DeleteFlag
					&& refSprite->BoundingRect.Width != -1
					&& maths::rectangle_clip(mainSprite->BoundingRect, refSprite->BoundingRect, nullptr))
				{
					spriteArr->push_back(refSprite);
				}
			}

			if (mainSprite->Bmp && spriteArr->size() < 2)
				spriteArr->clear();
			if (!spriteArr->empty())
			{
				mainSprite->OccludedSprites = spriteArr;
				spriteArr = nullptr;
			}
		}
	}

	delete spriteArr;
}

void render::SpriteViewer(bool* show)
{
	static const char* BitmapTypes[] =
	{
		"None",
		"RawBitmap",
		"DibBitmap",
		"Spliced",
	};
	static float scale = 1.0f;
	auto uv_min = ImVec2(0.0f, 0.0f); // Top-left
	auto uv_max = ImVec2(1.0f, 1.0f); // Lower-right
	auto tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
	auto border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white

	if (ImGui::Begin("Sprite viewer", show, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::SliderFloat("Sprite scale", &scale, 0.1f, 10.0f, "scale = %.3f");
			ImGui::EndMenuBar();
		}

		for (const auto group : pb::record_table->Groups)
		{
			bool emptyGroup = true;
			for (int i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (bmp)
				{
					emptyGroup = false;
					break;
				}
			}
			if (emptyGroup)
				continue;

			ImGui::Text("Group: %d, name:%s", group->GroupId, group->GroupName.c_str());
			for (int i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (!bmp)
					continue;

				auto type = BitmapTypes[static_cast<uint8_t>(bmp->BitmapType)];
				ImGui::Text("type:%s, size:%d, resolution: %dx%d, offset:%dx%d", type,
				            bmp->Resolution,
				            bmp->Width, bmp->Height, bmp->XPosition, bmp->YPosition);
			}

			for (int same = 0, i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (!bmp)
					continue;

				gdrv::CreatePreview(*bmp);
				if (bmp->Texture)
				{
					if (!same)
						same = true;
					else
						ImGui::SameLine();

					ImGui::Image(bmp->Texture, ImVec2(bmp->Width * scale, bmp->Height * scale),
					             uv_min, uv_max, tint_col, border_col);
				}
			}

			for (int same = 0, i = 0; i <= 2; i++)
			{
				auto zMap = group->GetZMap(i);
				if (!zMap)
					continue;

				zdrv::CreatePreview(*zMap);
				if (zMap->Texture)
				{
					if (!same)
						same = true;
					else
						ImGui::SameLine();
					ImGui::Image(zMap->Texture, ImVec2(zMap->Width * scale, zMap->Height * scale),
					             uv_min, uv_max, tint_col, border_col);
				}
			}
		}
	}
	ImGui::End();
}

// --- Display enhancements (Options > Graphics) ---------------------------------------------------------------
// Backdrop behind the table where the window is wider/taller than the table: 0 black, 1 ambient glow (the table,
// blurred and dimmed), 2 the table's art (its title screen or preview.bmp, see table_art; else ambient). Sharp scaling: blow the table up
// by a whole number with hard pixels, then smooth only the last step - crisp on big screens without blocky edges.
// Scanlines: optional CRT look.
static SDL_Texture* AmbientTexture = nullptr;
static SDL_Texture* ArtTexture = nullptr;
static bool ArtTried = false;
static SDL_Texture* SharpTexture = nullptr;
static int SharpW = 0, SharpH = 0;

static SDL_Texture* TableArt()
{
	if (!ArtTried)
	{
		ArtTried = true;
		if (auto surface = table_art::Load())
		{
			ArtTexture = SDL_CreateTextureFromSurface(winmain::Renderer, surface);
			SDL_FreeSurface(surface);
			if (ArtTexture)
				SDL_SetTextureScaleMode(ArtTexture, SDL_ScaleModeLinear);
		}
	}
	return ArtTexture;
}

static void DrawBackdrop(SDL_Texture* table)
{
	int outW, outH;
	SDL_GetRendererOutputSize(winmain::Renderer, &outW, &outH);
	auto& dst = render::DestinationRect;
	if (dst.w >= outW - 2 && dst.h >= outH - 2)
		return; // no bars to fill
	auto style = options::Options.Backdrop.V;
	if (style == 2)
	{
		TableArt();
		if (ArtTexture)
		{
			int w, h;
			SDL_QueryTexture(ArtTexture, nullptr, nullptr, &w, &h);
			// cover the window, keeping the picture's shape
			float scale = std::max(static_cast<float>(outW) / w, static_cast<float>(outH) / h);
			SDL_Rect cover{static_cast<int>(outW - w * scale) / 2, static_cast<int>(outH - h * scale) / 2,
			               static_cast<int>(w * scale), static_cast<int>(h * scale)};
			SDL_SetTextureColorMod(ArtTexture, 110, 110, 110);
			SDL_RenderCopy(winmain::Renderer, ArtTexture, nullptr, &cover);
			return;
		}
	}
	if (style == 0 || !SDL_RenderTargetSupported(winmain::Renderer))
		return;
	if (!AmbientTexture)
	{
		AmbientTexture = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 24, 18);
		if (!AmbientTexture)
			return;
		SDL_SetTextureScaleMode(AmbientTexture, SDL_ScaleModeLinear);
	}
	// Shrinking the table to 24x18 and stretching it back is a cheap, smooth blur
	auto previous = SDL_GetRenderTarget(winmain::Renderer);
	SDL_SetRenderTarget(winmain::Renderer, AmbientTexture);
	SDL_SetTextureScaleMode(table, SDL_ScaleModeLinear);
	SDL_RenderCopy(winmain::Renderer, table, nullptr, nullptr);
	SDL_SetRenderTarget(winmain::Renderer, previous);
	SDL_SetTextureColorMod(AmbientTexture, 70, 70, 80);
	SDL_RenderCopy(winmain::Renderer, AmbientTexture, nullptr, nullptr);
}

static bool DrawSharp(SDL_Texture* table, int srcW, int srcH)
{
	if (!options::Options.SharpScaling || !SDL_RenderTargetSupported(winmain::Renderer))
		return false;
	auto& dst = render::DestinationRect;
	int factor = static_cast<int>(std::ceil(std::max(static_cast<float>(dst.w) / srcW, static_cast<float>(dst.h) / srcH)));
	factor = std::max(1, std::min(factor, 4));
	if (factor == 1 || (dst.w == srcW * factor && dst.h == srcH * factor))
		return false; // nothing to smooth: plain copy is already exact
	if (!SharpTexture || SharpW != srcW * factor || SharpH != srcH * factor)
	{
		if (SharpTexture)
			SDL_DestroyTexture(SharpTexture);
		SharpW = srcW * factor;
		SharpH = srcH * factor;
		SharpTexture = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SharpW, SharpH);
		if (!SharpTexture)
			return false;
		SDL_SetTextureScaleMode(SharpTexture, SDL_ScaleModeLinear);
	}
	auto previous = SDL_GetRenderTarget(winmain::Renderer);
	SDL_SetRenderTarget(winmain::Renderer, SharpTexture);
	SDL_SetTextureScaleMode(table, SDL_ScaleModeNearest);
	SDL_RenderCopy(winmain::Renderer, table, nullptr, nullptr);
	SDL_SetRenderTarget(winmain::Renderer, previous);
	SDL_RenderCopy(winmain::Renderer, SharpTexture, nullptr, &dst);
	return true;
}

static void DrawScanlines(int srcH)
{
	if (!options::Options.Scanlines || srcH <= 0)
		return;
	auto& dst = render::DestinationRect;
	float rowH = static_cast<float>(dst.h) / srcH;
	if (rowH < 2.0f)
		return; // too small to show scanlines cleanly
	std::vector<SDL_Rect> lines;
	lines.reserve(srcH);
	auto lineH = std::max(1, static_cast<int>(rowH * 0.4f));
	for (int row = 0; row < srcH; row++)
		lines.push_back({dst.x, dst.y + static_cast<int>(row * rowH + rowH) - lineH, dst.w, lineH});
	SDL_SetRenderDrawBlendMode(winmain::Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(winmain::Renderer, 0, 0, 0, 90);
	SDL_RenderFillRects(winmain::Renderer, lines.data(), static_cast<int>(lines.size()));
}


// ---- Graphics effects (Options > Graphics). No shaders: only SDL blend modes and render targets, so they work
// on every renderer SDL picks. ----
static SDL_Texture* MakeTarget(SDL_Texture*& tex, int& curW, int& curH, int w, int h)
{
	w = std::max(1, w);
	h = std::max(1, h);
	if (tex && curW == w && curH == h)
		return tex;
	if (tex)
		SDL_DestroyTexture(tex);
	tex = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
	curW = w;
	curH = h;
	if (tex)
		SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
	return tex;
}

// Motion trails: each frame is blended over the last few, so moving things leave a short streak
static SDL_Texture* TrailTexture = nullptr;
static int TrailW = 0, TrailH = 0;
static bool TrailFresh = true;

static SDL_Texture* ApplyTrails(SDL_Texture* table, int w, int h)
{
	if (!options::Options.MotionTrails || !SDL_RenderTargetSupported(winmain::Renderer))
	{
		TrailFresh = true;
		return table;
	}
	auto created = TrailW != w || TrailH != h || !TrailTexture;
	if (!MakeTarget(TrailTexture, TrailW, TrailH, w, h))
		return table;
	SDL_SetTextureScaleMode(TrailTexture, SDL_ScaleModeNearest);
	auto previous = SDL_GetRenderTarget(winmain::Renderer);
	SDL_SetRenderTarget(winmain::Renderer, TrailTexture);
	SDL_BlendMode oldMode;
	SDL_GetTextureBlendMode(table, &oldMode);
	Uint8 oldAlpha;
	SDL_GetTextureAlphaMod(table, &oldAlpha);
	if (created || TrailFresh)
	{
		SDL_SetTextureBlendMode(table, SDL_BLENDMODE_NONE);
		SDL_RenderCopy(winmain::Renderer, table, nullptr, nullptr);
		TrailFresh = false;
	}
	else
	{
		SDL_SetTextureBlendMode(table, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(table, 150);
		SDL_RenderCopy(winmain::Renderer, table, nullptr, nullptr);
	}
	SDL_SetTextureAlphaMod(table, oldAlpha);
	SDL_SetTextureBlendMode(table, oldMode);
	SDL_SetRenderTarget(winmain::Renderer, previous);
	return TrailTexture;
}

// Glow: a bright-pass (the picture multiplied by itself, so only bright parts stay bright), blurred by shrinking,
// then added back on top
static SDL_Texture *GlowSmall = nullptr, *GlowBright = nullptr, *GlowBlur = nullptr;
static int GlowSmallW = 0, GlowSmallH = 0, GlowBrightW = 0, GlowBrightH = 0, GlowBlurW = 0, GlowBlurH = 0;

static void DrawGlow(SDL_Texture* table, int w, int h, const SDL_Rect& dst)
{
	auto level = options::Options.Glow.V;
	if (level <= 0 || !SDL_RenderTargetSupported(winmain::Renderer))
		return;
	if (!MakeTarget(GlowSmall, GlowSmallW, GlowSmallH, w / 4, h / 4) ||
		!MakeTarget(GlowBright, GlowBrightW, GlowBrightH, w / 4, h / 4) ||
		!MakeTarget(GlowBlur, GlowBlurW, GlowBlurH, w / 16, h / 16))
		return;
	auto r = winmain::Renderer;
	auto previous = SDL_GetRenderTarget(r);
	SDL_BlendMode oldMode;
	SDL_GetTextureBlendMode(table, &oldMode);
	SDL_ScaleMode oldScale;
	SDL_GetTextureScaleMode(table, &oldScale);

	SDL_SetRenderTarget(r, GlowSmall);
	SDL_SetTextureBlendMode(table, SDL_BLENDMODE_NONE);
	SDL_SetTextureScaleMode(table, SDL_ScaleModeLinear);
	SDL_RenderCopy(r, table, nullptr, nullptr);

	// Only lights glow. Mask the bright-pass source to the boxes of lit lights, bumpers and lane lights, then black
	// out flippers and balls on top: they are bright chrome and would otherwise bloom like lamps.
	{
		auto sx = static_cast<float>(GlowSmallW) / w, sy = static_cast<float>(GlowSmallH) / h;
		auto toSmall = [&](const rectangle_type& rc)
		{
			auto x0 = static_cast<int>(std::floor(rc.XPosition * sx)), y0 = static_cast<int>(std::floor(rc.YPosition * sy));
			auto x1 = static_cast<int>(std::ceil((rc.XPosition + rc.Width) * sx));
			auto y1 = static_cast<int>(std::ceil((rc.YPosition + rc.Height) * sy));
			return SDL_Rect{x0, y0, x1 - x0, y1 - y0};
		};
		std::vector<SDL_Rect> keep, cut;
		for (auto sprite : render::Sprites())
		{
			if (!sprite || sprite->DeleteFlag || !sprite->Bmp || sprite->BmpRect.Width <= 0)
				continue;
			if (sprite->GlowSource && sprite->VisualType == VisualTypes::Sprite)
				keep.push_back(toSmall(sprite->BmpRect));
			else if (sprite->IsFlipper || sprite->VisualType == VisualTypes::Ball)
				cut.push_back(toSmall(sprite->BmpRect));
		}
		// Mask texture: white where lights are, black elsewhere (and over flippers/balls); multiplied onto the source
		static SDL_Texture* mask = nullptr;
		static int maskW = 0, maskH = 0;
		if (MakeTarget(mask, maskW, maskH, GlowSmallW, GlowSmallH))
		{
			SDL_SetRenderTarget(r, mask);
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
			SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
			SDL_RenderClear(r);
			SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
			if (!keep.empty())
				SDL_RenderFillRects(r, keep.data(), static_cast<int>(keep.size()));
			SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
			if (!cut.empty())
				SDL_RenderFillRects(r, cut.data(), static_cast<int>(cut.size()));
			SDL_SetRenderTarget(r, GlowSmall);
			SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_MOD);
			SDL_RenderCopy(r, mask, nullptr, nullptr);
		}
		SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
	}

	SDL_SetRenderTarget(r, GlowBright);
	SDL_SetTextureBlendMode(GlowSmall, SDL_BLENDMODE_NONE);
	SDL_RenderCopy(r, GlowSmall, nullptr, nullptr);
	SDL_SetTextureBlendMode(GlowSmall, SDL_BLENDMODE_MOD);
	SDL_RenderCopy(r, GlowSmall, nullptr, nullptr); // squared
	SDL_RenderCopy(r, GlowSmall, nullptr, nullptr); // cubed: only real highlights are left

	SDL_SetRenderTarget(r, GlowBlur);
	SDL_SetTextureBlendMode(GlowBright, SDL_BLENDMODE_NONE);
	SDL_RenderCopy(r, GlowBright, nullptr, nullptr);

	SDL_SetRenderTarget(r, previous);
	Uint8 wide = level == 1 ? 150 : 255, tight = level == 1 ? 70 : 130;
	SDL_SetTextureBlendMode(GlowBlur, SDL_BLENDMODE_ADD);
	SDL_SetTextureColorMod(GlowBlur, wide, wide, wide);
	SDL_RenderCopy(r, GlowBlur, nullptr, &dst);
	SDL_SetTextureBlendMode(GlowBright, SDL_BLENDMODE_ADD);
	SDL_SetTextureColorMod(GlowBright, tight, tight, tight);
	SDL_RenderCopy(r, GlowBright, nullptr, &dst);
	SDL_SetTextureColorMod(GlowBright, 255, 255, 255);

	SDL_SetTextureBlendMode(table, oldMode);
	SDL_SetTextureScaleMode(table, oldScale);
}

// Brightness: the table added on top of itself at a fraction (a multiply that keeps blacks black)
static void DrawBrightness(SDL_Texture* table, const SDL_Rect& dst)
{
	auto level = options::Options.Brightness.V;
	if (level <= 0)
		return;
	SDL_BlendMode oldMode;
	SDL_GetTextureBlendMode(table, &oldMode);
	Uint8 amount = level == 1 ? 45 : 95;
	SDL_SetTextureBlendMode(table, SDL_BLENDMODE_ADD);
	SDL_SetTextureColorMod(table, amount, amount, amount);
	SDL_RenderCopy(winmain::Renderer, table, nullptr, &dst);
	SDL_SetTextureColorMod(table, 255, 255, 255);
	SDL_SetTextureBlendMode(table, oldMode);
}

static SDL_Texture* MakeOverlay(Uint32 (*pixel)(float x, float y))
{
	const int size = 256;
	auto surface = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!surface)
		return nullptr;
	auto pixels = static_cast<Uint32*>(surface->pixels);
	for (int y = 0; y < size; y++)
		for (int x = 0; x < size; x++)
			pixels[y * (surface->pitch / 4) + x] = pixel((x + 0.5f) / size, (y + 0.5f) / size);
	auto tex = SDL_CreateTextureFromSurface(winmain::Renderer, surface);
	SDL_FreeSurface(surface);
	if (tex)
	{
		SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
	}
	return tex;
}

static SDL_Texture* VignetteTexture = nullptr;

static void DrawVignette(const SDL_Rect& dst)
{
	if (!options::Options.Vignette)
		return;
	if (!VignetteTexture)
		VignetteTexture = MakeOverlay([](float x, float y) -> Uint32
		{
			auto dx = (x - 0.5f) * 2.0f, dy = (y - 0.5f) * 2.0f;
			auto d = std::sqrt(dx * dx * 0.9f + dy * dy * 0.7f);
			auto t = std::min(1.0f, std::max(0.0f, (d - 0.55f) / 0.75f));
			auto a = static_cast<Uint32>(150.0f * t * t * (3 - 2 * t));
			return a << 24;
		});
	if (VignetteTexture)
		SDL_RenderCopy(winmain::Renderer, VignetteTexture, nullptr, &dst);
}

// ---------------------------------------------------------------------------------------------------------------
// Upscaled table art: Scale2x (also known as EPX), a pixel-art upscaler that rounds off staircase edges and diagonals
// without blurring. It runs on the finished table picture each frame (cheap integer compares); 4x is two passes.
// ---------------------------------------------------------------------------------------------------------------
static std::vector<ColorRgba> UpscaleBuf[2];
static SDL_Texture* UpscaleTexture = nullptr;
static int UpscaleW = 0, UpscaleH = 0;

// Half way between two colours (the smooth variant puts this in the corner pixels Scale2x would copy over)
static uint32_t Mix(uint32_t a, uint32_t b)
{
	return ((a & 0xFEFEFEFEu) >> 1) + ((b & 0xFEFEFEFEu) >> 1) + (a & b & 0x01010101u);
}

// smooth: the rounded-off corners are blended half way with the pixel's own colour, so diagonals come out
// anti-aliased instead of in hard steps, while flat areas and straight edges stay exactly as sharp
static void Scale2x(const ColorRgba* src, int w, int h, ColorRgba* dst, bool smooth = false)
{
	for (int y = 0; y < h; y++)
	{
		const ColorRgba* row = src + static_cast<size_t>(y) * w;
		const ColorRgba* up = y > 0 ? row - w : row;
		const ColorRgba* down = y < h - 1 ? row + w : row;
		ColorRgba* d0 = dst + static_cast<size_t>(2 * y) * (2 * w);
		ColorRgba* d1 = d0 + 2 * w;
		for (int x = 0; x < w; x++)
		{
			uint32_t e = row[x].Color, b = up[x].Color, hh = down[x].Color;
			uint32_t d = row[x > 0 ? x - 1 : x].Color, f = row[x < w - 1 ? x + 1 : x].Color;
			uint32_t e0 = e, e1 = e, e2 = e, e3 = e;
			if (b != hh && d != f)
			{
				if (d == b) e0 = smooth ? Mix(d, e) : d;
				if (b == f) e1 = smooth ? Mix(f, e) : f;
				if (d == hh) e2 = smooth ? Mix(d, e) : d;
				if (hh == f) e3 = smooth ? Mix(f, e) : f;
			}
			d0[2 * x].Color = e0;
			d0[2 * x + 1].Color = e1;
			d1[2 * x].Color = e2;
			d1[2 * x + 1].Color = e3;
		}
	}
}

// The texture to draw the table from: the plain vscreen texture, or its Scale2x upscale. factor = size multiplier.
static SDL_Texture* UpscaleArt(gdrv_bitmap8* screen, const ColorRgba* frame, int& factor)
{
	factor = 1;
	// 1 = 2x, 2 = 4x, 3 = 2x smooth, 4 = 4x smooth (the smooth blend on the first pass only: a second blend
	// would soften the art)
	int mode = options::Options.UpscaleArt;
	bool smooth = mode >= 3;
	int passes = smooth ? mode - 2 : std::min(mode, 2);
	if (passes <= 0)
		return screen->Texture;
	int tw = screen->Width << passes, th = screen->Height << passes;
	if (!UpscaleTexture || UpscaleW != tw || UpscaleH != th)
	{
		if (UpscaleTexture)
			SDL_DestroyTexture(UpscaleTexture);
		UpscaleTexture = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, tw, th);
		UpscaleW = tw;
		UpscaleH = th;
		if (!UpscaleTexture)
			return screen->Texture;
		SDL_SetTextureBlendMode(UpscaleTexture, SDL_BLENDMODE_NONE);
	}
	const ColorRgba* src = frame;
	int cw = screen->Width, ch = screen->Height;
	for (int p = 0; p < passes; p++)
	{
		auto& out = UpscaleBuf[p];
		out.resize(static_cast<size_t>(cw) * ch * 4);
		Scale2x(src, cw, ch, out.data(), smooth && p == 0);
		src = out.data();
		cw *= 2;
		ch *= 2;
	}
	SDL_UpdateTexture(UpscaleTexture, nullptr, src, cw * static_cast<int>(sizeof(ColorRgba)));
	factor = 1 << passes;
	return UpscaleTexture;
}

// ---------------------------------------------------------------------------------------------------------------
// Ball shadow and gloss reflection. They are painted into a copy of the table picture, pixel by pixel, with the same
// depth test the ball itself gets (zdrv::paint_flat: a ball pixel shows where the depth buffer is farther away than
// the ball). So wherever a ramp, wire rail or plastic would hide the ball, it hides the shadow too. The ball's own
// pixels are never touched. Light comes from the upper left, as in the table art.
// ---------------------------------------------------------------------------------------------------------------
static std::vector<ColorRgba> ShadowFrame;

static float SmoothStep(float a, float b, float x)
{
	float t = std::min(1.0f, std::max(0.0f, (x - a) / (b - a)));
	return t * t * (3 - 2 * t);
}

// 0 inside the ball, 1 outside it (soft edge); coordinates in ball diameters from the ball centre
static float OutsideBall(float px, float py)
{
	return SmoothStep(0.46f, 0.52f, std::sqrt(px * px + py * py));
}

// Returns the picture to show: the table picture itself, or a copy with ball shadows when there is a ball to shade
static const ColorRgba* ApplyBallShadows(gdrv_bitmap8* screen)
{
	auto z = render::ZScreen();
	if (!options::Options.BallShadow || !z || z->Width != screen->Width || z->Height != screen->Height)
		return screen->BmpBufPtr1;
	bool anyBall = false;
	for (auto ball : render::Balls())
		anyBall |= ball && !ball->DeleteFlag && ball->Bmp && ball->BmpRect.Width > 0;
	if (!anyBall)
		return screen->BmpBufPtr1;

	ShadowFrame.assign(screen->BmpBufPtr1, screen->BmpBufPtr1 + static_cast<size_t>(screen->Stride) * screen->Height);
	for (auto ball : render::Balls())
	{
		if (!ball || ball->DeleteFlag || !ball->Bmp || ball->BmpRect.Width <= 0)
			continue;
		auto& r = ball->BmpRect;
		float d = static_cast<float>(std::max(r.Width, r.Height));
		float cx = r.XPosition + r.Width * 0.5f, cy = r.YPosition + r.Height * 0.5f;
		int x0 = std::max(0, static_cast<int>(cx - d)), x1 = std::min(screen->Width, static_cast<int>(cx + d) + 1);
		int y0 = std::max(0, static_cast<int>(cy - d)), y1 = std::min(screen->Height, static_cast<int>(cy + d) + 1);
		for (int y = y0; y < y1; y++)
			for (int x = x0; x < x1; x++)
			{
				float px = (x + 0.5f - cx) / d, py = (y + 0.5f - cy) / d;
				float outside = OutsideBall(px, py);
				if (outside <= 0)
					continue; // the ball itself
				if (z->ZPtr1[static_cast<size_t>(y) * z->Stride + x] <= ball->Depth)
					continue; // something nearer covers this spot: the ball would be hidden here, so is its shadow
				float sx = px - 0.34f, sy = py - 0.44f;
				float shade = 0.62f * (1 - SmoothStep(0.22f, 0.66f, std::sqrt(sx * sx + sy * sy))) * outside;
				float qx = px / 0.32f, qy = (py - 0.70f) / 0.14f, q = qx * qx + qy * qy;
				float gloss = q < 1 ? 0.32f * (1 - q) * outside : 0.0f;
				if (shade < 0.004f && gloss < 0.004f)
					continue;
				auto& c = ShadowFrame[static_cast<size_t>(y) * screen->Stride + x];
				float red = c.GetRed() * (1 - shade), green = c.GetGreen() * (1 - shade), blue = c.GetBlue() * (1 - shade);
				red += (255 - red) * gloss;
				green += (255 - green) * gloss;
				blue += (255 - blue) * gloss;
				c.SetRed(static_cast<uint8_t>(red));
				c.SetGreen(static_cast<uint8_t>(green));
				c.SetBlue(static_cast<uint8_t>(blue));
			}
	}
	return ShadowFrame.data();
}

// ---------------------------------------------------------------------------------------------------------------
// Light pools: every lit lamp (the same lights the Glow option uses) casts a large, soft pool of its own colour onto
// the artwork around it. The colour is the average of the lamp's bright pixels in the current picture.
// ---------------------------------------------------------------------------------------------------------------
static SDL_Texture* LightPoolTexture = nullptr;

static void DrawLightPools(const SDL_Rect& dst)
{
	int level = options::Options.LightPools;
	auto screen = render::vscreen;
	if (level <= 0 || screen->Width <= 0)
		return;
	if (!LightPoolTexture)
	{
		LightPoolTexture = MakeOverlay([](float x, float y) -> Uint32
		{
			float dx = (x - 0.5f) * 2, dy = (y - 0.5f) * 2;
			float d = std::min(1.0f, std::sqrt(dx * dx + dy * dy));
			float a = (1 - d) * (1 - d);
			return static_cast<Uint32>(a * 255) << 24 | 0xFFFFFFu;
		});
		if (!LightPoolTexture)
			return;
		SDL_SetTextureBlendMode(LightPoolTexture, SDL_BLENDMODE_ADD);
	}
	float sx = static_cast<float>(dst.w) / screen->Width, sy = static_cast<float>(dst.h) / screen->Height;
	const ColorRgba* pixels = screen->BmpBufPtr1;
	for (auto sprite : render::Sprites())
	{
		if (!sprite || sprite->DeleteFlag || !sprite->Bmp || sprite->BmpRect.Width <= 0 || !sprite->GlowSource ||
			sprite->VisualType != VisualTypes::Sprite)
			continue;
		auto& r = sprite->BmpRect;
		long red = 0, green = 0, blue = 0, count = 0;
		for (int y = std::max(0, r.YPosition); y < std::min(screen->Height, r.YPosition + r.Height); y += 2)
			for (int x = std::max(0, r.XPosition); x < std::min(screen->Width, r.XPosition + r.Width); x += 2)
			{
				auto c = pixels[static_cast<size_t>(y) * screen->Width + x];
				if (c.GetRed() + c.GetGreen() + c.GetBlue() > 300)
				{
					red += c.GetRed();
					green += c.GetGreen();
					blue += c.GetBlue();
					count++;
				}
			}
		if (!count)
			continue; // lamp not lit right now (only dark pixels)
		// big sprites (bumper caps) would flood the table: cap the lamp size the pool is based on
		float size = std::min(std::max(r.Width, r.Height), 22) * (level == 1 ? 2.6f : 3.3f);
		float cx = dst.x + (r.XPosition + r.Width * 0.5f) * sx, cy = dst.y + (r.YPosition + r.Height * 0.5f) * sy;
		SDL_Rect area{static_cast<int>(cx - size * sx / 2), static_cast<int>(cy - size * sy / 2),
		              static_cast<int>(size * sx), static_cast<int>(size * sy)};
		SDL_SetTextureColorMod(LightPoolTexture, static_cast<Uint8>(red / count), static_cast<Uint8>(green / count),
		                       static_cast<Uint8>(blue / count));
		SDL_SetTextureAlphaMod(LightPoolTexture, level == 1 ? 50 : 85);
		SDL_RenderCopy(winmain::Renderer, LightPoolTexture, nullptr, &area);
	}
}

void render::PresentVScreen()
{
	auto frame = ApplyBallShadows(vscreen);
	if (frame == vscreen->BmpBufPtr1)
		vscreen->BlitToTexture();
	else
		SDL_UpdateTexture(vscreen->Texture, nullptr, frame, vscreen->Width * static_cast<int>(sizeof(ColorRgba)));
	int artScale = 1;
	auto artTexture = UpscaleArt(vscreen, frame, artScale);
	auto tableTexture = ApplyTrails(artTexture, vscreen->Width * artScale, vscreen->Height * artScale);
	DrawBackdrop(tableTexture);

	if (offset_x == 0 && offset_y == 0)
	{
		if (!DrawSharp(tableTexture, vscreen->Width * artScale, vscreen->Height * artScale))
		{
			SDL_SetTextureScaleMode(tableTexture,
			                        options::Options.LinearFiltering ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
			SDL_RenderCopy(winmain::Renderer, tableTexture, nullptr, &DestinationRect);
		}
	}
	else
	{
		auto tableWidthCoef = static_cast<float>(pb::MainTable->Width) / vscreen->Width;
		auto srcSeparationX = static_cast<int>(round(vscreen->Width * tableWidthCoef));
		auto srcBoardRect = SDL_Rect
		{
			0, 0,
			srcSeparationX * artScale, vscreen->Height * artScale
		};
		auto srcSidebarRect = SDL_Rect
		{
			srcSeparationX * artScale, 0,
			(vscreen->Width - srcSeparationX) * artScale, vscreen->Height * artScale
		};

#if SDL_VERSION_ATLEAST(2, 0, 10)
		// SDL_RenderCopyF was added in 2.0.10
		auto dstSeparationX = DestinationRect.w * tableWidthCoef;
		auto dstBoardRect = SDL_FRect
		{
			DestinationRect.x + offset_x * fullscrn::ScaleX,
			DestinationRect.y + offset_y * fullscrn::ScaleY,
			dstSeparationX, static_cast<float>(DestinationRect.h)
		};
		auto dstSidebarRect = SDL_FRect
		{
			DestinationRect.x + dstSeparationX, static_cast<float>(DestinationRect.y),
			DestinationRect.w - dstSeparationX, static_cast<float>(DestinationRect.h)
		};

		SDL_RenderCopyF(winmain::Renderer, tableTexture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopyF(winmain::Renderer, tableTexture, &srcSidebarRect, &dstSidebarRect);
#else
		// SDL_RenderCopy cannot express sub pixel offset.
		// Vscreen shift is required for that.
		auto dstSeparationX = static_cast<int>(DestinationRect.w * tableWidthCoef);
		auto scaledOffX = static_cast<int>(round(offset_x * fullscrn::ScaleX));
		if (offset_x != 0 && scaledOffX == 0)
			scaledOffX = Sign(offset_x);
		auto scaledOffY = static_cast<int>(round(offset_y * fullscrn::ScaleY));
		if (offset_y != 0 && scaledOffX == 0)
			scaledOffY = Sign(offset_y);

		auto dstBoardRect = SDL_Rect
		{
			DestinationRect.x + scaledOffX, DestinationRect.y + scaledOffY,
			dstSeparationX, DestinationRect.h
		};
		auto dstSidebarRect = SDL_Rect
		{
			DestinationRect.x + dstSeparationX, DestinationRect.y,
			DestinationRect.w - dstSeparationX, DestinationRect.h
		};

		SDL_RenderCopy(winmain::Renderer, tableTexture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopy(winmain::Renderer, tableTexture, &srcSidebarRect, &dstSidebarRect);
#endif
	}

	DrawBrightness(tableTexture, DestinationRect);
	DrawGlow(tableTexture, vscreen->Width, vscreen->Height, DestinationRect);
	DrawLightPools(DestinationRect);
	DrawVignette(DestinationRect);
	DrawScanlines(vscreen->Height);
	side_panel::Draw();

	if (options::Options.DebugOverlay)
	{
		DebugOverlay::DrawOverlay();
	}
}
