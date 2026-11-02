#include "pch.h"
#include "render.h"
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
	if (getenv("FT2_NOZ")) Depth = 0;
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
// blurred and dimmed), 2 the table's art (preview.bmp in its folder, else ambient). Sharp scaling: blow the table up
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
		if (auto surface = SDL_LoadBMP("preview.bmp"))
		{
			ArtTexture = SDL_CreateTextureFromSurface(winmain::Renderer, surface);
			SDL_FreeSurface(surface);
			if (ArtTexture)
				SDL_SetTextureScaleMode(ArtTexture, SDL_ScaleModeLinear);
		}
	}
	return ArtTexture;
}

static void DrawSidePanel()
{
	auto& panel = render::SidePanelRect;
	if (panel.w <= 0 || !pb::MainTable)
		return;
	SDL_SetRenderDrawBlendMode(winmain::Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(winmain::Renderer, 8, 8, 14, 230);
	SDL_RenderFillRect(winmain::Renderer, &panel);
	// Art on top (the table's preview.bmp), keeping its shape
	float y = static_cast<float>(panel.y) + panel.h * 0.03f;
	if (auto art = TableArt())
	{
		int w, h;
		SDL_QueryTexture(art, nullptr, nullptr, &w, &h);
		float scale = std::min(panel.w * 0.92f / w, panel.h * 0.42f / h);
		SDL_Rect dst{panel.x + static_cast<int>((panel.w - w * scale) / 2), static_cast<int>(y),
		             static_cast<int>(w * scale), static_cast<int>(h * scale)};
		SDL_SetTextureColorMod(art, 255, 255, 255);
		SDL_RenderCopy(winmain::Renderer, art, nullptr, &dst);
		y += dst.h + panel.h * 0.04f;
	}
	// Score, ball and player, Full Tilt 1 style
	auto draw = ImGui::GetForegroundDrawList();
	auto font = ImGui::GetFont();
	float big = panel.h * 0.075f, small = panel.h * 0.04f;
	auto line = [&](const std::string& text, float size, ImU32 color)
	{
		auto textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
		draw->AddText(font, size, ImVec2(panel.x + (panel.w - textSize.x) / 2, y), color, text.c_str());
		y += textSize.y * 1.15f;
	};
	auto table = pb::MainTable;
	std::string score = std::to_string(table->CurScoreStruct ? table->CurScoreStruct->Score : 0);
	for (int i = static_cast<int>(score.size()) - 3; i > 0; i -= 3)
		score.insert(static_cast<size_t>(i), ",");
	line("Player " + std::to_string(table->CurrentPlayer + 1), small, IM_COL32(200, 200, 200, 255));
	line(score, big, IM_COL32(255, 210, 80, 255));
	line("Ball " + std::to_string(std::max(1, table->MaxBallCount - table->BallCount + 1)), small, IM_COL32(200, 200, 200, 255));
	y += small * 0.6f;
	if (pb::InfoTextBox && pb::InfoTextBox->CurrentMessage)
	{
		auto wrapW = panel.w * 0.9f;
		auto text = pb::InfoTextBox->CurrentMessage->Text;
		auto textSize = font->CalcTextSizeA(small, FLT_MAX, wrapW, text);
		draw->AddText(font, small, ImVec2(panel.x + (panel.w - std::min(wrapW, textSize.x)) / 2, y),
		              IM_COL32(120, 220, 255, 255), text, nullptr, wrapW);
	}
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

void render::PresentVScreen()
{
	vscreen->BlitToTexture();
	auto tableTexture = ApplyTrails(vscreen->Texture, vscreen->Width, vscreen->Height);
	DrawBackdrop(tableTexture);

	if (offset_x == 0 && offset_y == 0)
	{
		if (!DrawSharp(tableTexture, vscreen->Width, vscreen->Height))
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
			srcSeparationX, vscreen->Height
		};
		auto srcSidebarRect = SDL_Rect
		{
			srcSeparationX, 0,
			vscreen->Width - srcSeparationX, vscreen->Height
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
	DrawVignette(DestinationRect);
	DrawScanlines(vscreen->Height);
	DrawSidePanel();

	if (options::Options.DebugOverlay)
	{
		DebugOverlay::DrawOverlay();
	}
}
