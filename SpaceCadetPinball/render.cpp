#include "pch.h"
#include "render.h"

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

	if (AllowFade && bmp != Bmp && options::Options.SmoothLights)
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

static constexpr uint32_t LightFadeMs = 70;

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
		auto t = static_cast<float>(now - sprite->FadeStart) / LightFadeMs;
		if (t >= 1.0f || !options::Options.SmoothLights)
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

void render::PresentVScreen()
{
	vscreen->BlitToTexture();
	DrawBackdrop(vscreen->Texture);

	if (offset_x == 0 && offset_y == 0)
	{
		if (!DrawSharp(vscreen->Texture, vscreen->Width, vscreen->Height))
		{
			SDL_SetTextureScaleMode(vscreen->Texture,
			                        options::Options.LinearFiltering ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
			SDL_RenderCopy(winmain::Renderer, vscreen->Texture, nullptr, &DestinationRect);
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

		SDL_RenderCopyF(winmain::Renderer, vscreen->Texture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopyF(winmain::Renderer, vscreen->Texture, &srcSidebarRect, &dstSidebarRect);
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

		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcSidebarRect, &dstSidebarRect);
#endif
	}

	DrawScanlines(vscreen->Height);

	if (options::Options.DebugOverlay)
	{
		DebugOverlay::DrawOverlay();
	}
}
