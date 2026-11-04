#include "pch.h"
#include <sstream>
#include "side_panel.h"

#include "ft2_control.h"
#include "high_score.h"
#include "imgui.h"
#include "options.h"
#include "pb.h"
#include "render.h"
#include "score.h"
#include "table_art.h"
#include "TPinballTable.h"
#include "TTextBox.h"
#include "TTextBoxMessage.h"
#include "winmain.h"

namespace
{
	// ---- Table art: the table's title screen (see table_art), and a tiny copy of it that, stretched over the panel,
	// is a soft blur of the same colours: the backdrop the picture fades into ----
	SDL_Texture* ArtTexture = nullptr;
	SDL_Texture* BlurTexture = nullptr;

	SDL_Texture* art()
	{
		static bool tried = false;
		if (!tried)
		{
			tried = true;
			if (auto surface = table_art::Load())
			{
				ArtTexture = SDL_CreateTextureFromSurface(winmain::Renderer, surface);
				if (ArtTexture)
					SDL_SetTextureScaleMode(ArtTexture, SDL_ScaleModeLinear);
				if (auto rgb = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0))
				{
					if (auto tiny = SDL_CreateRGBSurfaceWithFormat(0, 12, 9, 32, SDL_PIXELFORMAT_ARGB8888))
					{
						SDL_BlitScaled(rgb, nullptr, tiny, nullptr);
						BlurTexture = SDL_CreateTextureFromSurface(winmain::Renderer, tiny);
						if (BlurTexture)
							SDL_SetTextureScaleMode(BlurTexture, SDL_ScaleModeLinear);
						SDL_FreeSurface(tiny);
					}
					SDL_FreeSurface(rgb);
				}
				SDL_FreeSurface(surface);
			}
		}
		return ArtTexture;
	}

	// ---- Full Tilt 1-style frames: a bevelled gold border, light along the top and left, dark along the bottom
	// and right, shaded across its width like a rounded metal edge, with a dark line on both sides ----
	void bevel(const SDL_Rect& r, int thickness)
	{
		auto renderer = winmain::Renderer;
		for (int i = 0; i < thickness; i++)
		{
			SDL_Rect ring{r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i};
			if (ring.w <= 1 || ring.h <= 1)
				break;
			Uint8 lr, lg, lb, dr, dg, db;
			if (i == 0 || (i == thickness - 1 && thickness >= 3))
			{
				lr = dr = 24;
				lg = dg = 16;
				lb = db = 6;
			}
			else
			{
				auto f = thickness >= 3 ? static_cast<float>(i) / (thickness - 1) : 0.5f;
				auto shine = 0.55f + 0.45f * std::sin(f * 3.14159f); // brightest in the middle of the edge
				lr = static_cast<Uint8>(235 * shine);
				lg = static_cast<Uint8>(190 * shine);
				lb = static_cast<Uint8>(95 * shine);
				dr = static_cast<Uint8>(140 * shine);
				dg = static_cast<Uint8>(96 * shine);
				db = static_cast<Uint8>(38 * shine);
			}
			int x0 = ring.x, y0 = ring.y, x1 = ring.x + ring.w - 1, y1 = ring.y + ring.h - 1;
			SDL_SetRenderDrawColor(renderer, lr, lg, lb, 255);
			SDL_RenderDrawLine(renderer, x0, y0, x1, y0);
			SDL_RenderDrawLine(renderer, x0, y0, x0, y1);
			SDL_SetRenderDrawColor(renderer, dr, dg, db, 255);
			SDL_RenderDrawLine(renderer, x0, y1, x1, y1);
			SDL_RenderDrawLine(renderer, x1, y0, x1, y1);
		}
	}

	// A black inset box (score, messages) inside a bevel, like the boxes of the Full Tilt 1 panels
	void inset_box(const SDL_Rect& r, int thickness, Uint8 alpha = 215)
	{
		SDL_SetRenderDrawColor(winmain::Renderer, 4, 3, 2, alpha);
		SDL_Rect inside{r.x + thickness, r.y + thickness, r.w - 2 * thickness, r.h - 2 * thickness};
		SDL_RenderFillRect(winmain::Renderer, &inside);
		bevel(r, thickness);
	}

	// ---- Dot-matrix display: 128 x 32 dots, 5 x 7 font (columns, bit 0 = top row), ASCII ' ' to 'Z' ----
	const uint8_t Font5x7[][5] =
	{
		{0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},
		{0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
		{0x36, 0x49, 0x56, 0x20, 0x50}, {0x00, 0x08, 0x07, 0x03, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},
		{0x00, 0x41, 0x22, 0x1C, 0x00}, {0x2A, 0x1C, 0x7F, 0x1C, 0x2A}, {0x08, 0x08, 0x3E, 0x08, 0x08},
		{0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
		{0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
		{0x72, 0x49, 0x49, 0x49, 0x46}, {0x21, 0x41, 0x49, 0x4D, 0x33}, {0x18, 0x14, 0x12, 0x7F, 0x10},
		{0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x31}, {0x41, 0x21, 0x11, 0x09, 0x07},
		{0x36, 0x49, 0x49, 0x49, 0x36}, {0x46, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x00, 0x14, 0x00, 0x00},
		{0x00, 0x40, 0x34, 0x00, 0x00}, {0x00, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14},
		{0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x59, 0x09, 0x06}, {0x3E, 0x41, 0x5D, 0x59, 0x4E},
		{0x7C, 0x12, 0x11, 0x12, 0x7C}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
		{0x7F, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
		{0x3E, 0x41, 0x41, 0x51, 0x73}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
		{0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
		{0x7F, 0x02, 0x1C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
		{0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
		{0x26, 0x49, 0x49, 0x49, 0x32}, {0x03, 0x01, 0x7F, 0x01, 0x03}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
		{0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
		{0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x59, 0x49, 0x4D, 0x43},
	};

	const int DmdW = 128, DmdH = 32;

	struct Dmd
	{
		uint8_t Dots[DmdH][DmdW]{};

		static const uint8_t* glyph(char c)
		{
			c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
			if (c < ' ' || c > 'Z')
				c = ' ';
			return Font5x7[c - ' '];
		}

		// Width in dots of a text at a scale (each glyph 5 wide plus 1 gap)
		static int width(const std::string& text, int scale)
		{
			return text.empty() ? 0 : static_cast<int>(text.size()) * 6 * scale - scale;
		}

		void text(const std::string& text, int x, int y, int scale, uint8_t level = 255)
		{
			for (auto c : text)
			{
				auto g = glyph(c);
				for (int col = 0; col < 5; col++)
					for (int row = 0; row < 7; row++)
						if (g[col] >> row & 1)
							for (int dy = 0; dy < scale; dy++)
								for (int dx = 0; dx < scale; dx++)
								{
									int px = x + col * scale + dx, py = y + row * scale + dy;
									if (px >= 0 && px < DmdW && py >= 0 && py < DmdH)
										Dots[py][px] = level;
								}
				x += 6 * scale;
			}
		}

		void centred(const std::string& line, int y, int scale, uint8_t level = 255)
		{
			text(line, (DmdW - width(line, scale)) / 2, y, scale, level);
		}

		// Word-wrap to the display width (21 characters at scale 1)
		static std::vector<std::string> wrap(const std::string& message, size_t columns)
		{
			std::vector<std::string> lines;
			std::istringstream words(message);
			std::string word, line;
			while (words >> word)
			{
				if (!line.empty() && line.size() + 1 + word.size() > columns)
				{
					lines.push_back(line);
					line.clear();
				}
				line += (line.empty() ? "" : " ") + word.substr(0, columns);
			}
			if (!line.empty())
				lines.push_back(line);
			return lines;
		}

		// Drawn at a whole number of screen pixels per dot (copied 1:1, so the dots stay even at any size): the dot,
		// then a dark gap; from 4 pixels a dot the corners are softened. Centred in w. Returns the height used.
		static int pitch(float w)
		{
			return std::max(2, static_cast<int>(w / DmdW));
		}

		float draw(float x, float y, float w)
		{
			int k = pitch(w);
			static SDL_Texture* texture = nullptr;
			static int textureK = 0;
			static std::vector<uint32_t> pixels;
			if (!texture || textureK != k)
			{
				if (texture)
					SDL_DestroyTexture(texture);
				texture = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
				                            DmdW * k, DmdH * k);
				textureK = k;
				if (!texture)
					return 0.0f;
				SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
				pixels.assign(static_cast<size_t>(DmdW * k * DmdH * k), 0xFF0C0602);
			}
			const uint32_t gap = 0xFF0C0602;
			auto dot = k == 2 ? 2 : k - 1;
			for (int row = 0; row < DmdH; row++)
				for (int col = 0; col < DmdW; col++)
				{
					auto level = Dots[row][col];
					uint32_t r = level ? 255u * level / 255 : 52, g = level ? 20u + 120u * level / 255 : 24,
					         b = level ? 10u : 8;
					uint32_t on = 0xFF000000u | r << 16 | g << 8 | b;
					uint32_t soft = 0xFF000000u | (r * 5 / 8) << 16 | (g * 5 / 8) << 8 | (b * 5 / 8);
					for (int dy = 0; dy < k; dy++)
						for (int dx = 0; dx < k; dx++)
						{
							uint32_t color = gap;
							if (dx < dot && dy < dot)
							{
								auto corner = (dx == 0 || dx == dot - 1) && (dy == 0 || dy == dot - 1);
								color = k == 2 ? (dx == 0 && dy == 0 ? on : soft) : k >= 4 && corner ? soft : on;
							}
							pixels[static_cast<size_t>((row * k + dy) * DmdW * k + col * k + dx)] = color;
						}
				}
			SDL_UpdateTexture(texture, nullptr, pixels.data(), DmdW * k * 4);
			int dw = DmdW * k, dh = DmdH * k;
			SDL_Rect dst{static_cast<int>(x + (w - dw) / 2), static_cast<int>(y), dw, dh};
			SDL_RenderCopy(winmain::Renderer, texture, nullptr, &dst);
			return static_cast<float>(dh);
		}
	};

	std::string with_commas(long long value)
	{
		auto digits = std::to_string(value);
		for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
			digits.insert(static_cast<size_t>(i), ",");
		return digits;
	}

	long long player_score(TPinballTable* table, int player)
	{
		if (player == table->CurrentPlayer)
			return static_cast<long long>(table->CurScoreE9) * 1000000000LL + table->CurScore;
		auto& saved = table->PlayerScores[player];
		return static_cast<long long>(saved.ScoreE9Part) * 1000000000LL + saved.Score;
	}

	// The message the table is showing now, if any: the rules' timed messages, and once the game is over the
	// standing text (GAME OVER). During play the standing text is just "Player 1", which the display shows anyway.
	const char* live_message()
	{
		for (auto box : {pb::InfoTextBox, pb::MissTextBox})
		{
			if (!box || !box->CurrentMessage)
				continue;
			auto message = box->CurrentMessage;
			if (message->Time == -1.0f ? pb::game_mode != GameModes::InGame : message->TimeLeft() > 0.0f)
				return message->Text;
		}
		return nullptr;
	}
}

void side_panel::Draw()
{
	auto& panel = render::SidePanelRect;
	auto table = pb::MainTable;
	if (panel.w <= 0 || !table)
		return;
	auto renderer = winmain::Renderer;
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	// Backdrop: the art's own colours, blurred and dark, under everything (black without art)
	auto texture = art();
	SDL_SetRenderDrawColor(renderer, 6, 5, 8, 255);
	SDL_RenderFillRect(renderer, &panel);
	if (BlurTexture)
	{
		SDL_SetTextureColorMod(BlurTexture, 70, 70, 76);
		SDL_RenderCopy(renderer, BlurTexture, nullptr, &panel);
	}

	int edge = std::max(3, std::min(8, panel.w / 55)); // frame thickness
	SDL_Rect inner{panel.x + edge, panel.y + edge, panel.w - 2 * edge, panel.h - 2 * edge};
	float y = static_cast<float>(inner.y);

	// The title screen across the full width at the top; its lower part fades into the backdrop
	if (texture)
	{
		int w, h;
		SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
		int artH = std::min(static_cast<int>(static_cast<float>(inner.w) * h / w), static_cast<int>(panel.h * 0.42f));
		int fadeH = artH / 4;
		SDL_SetTextureColorMod(texture, 255, 255, 255);
		SDL_SetTextureAlphaMod(texture, 255);
		SDL_Rect src{0, 0, w, h - h * fadeH / artH};
		SDL_Rect dst{inner.x, inner.y, inner.w, artH - fadeH};
		SDL_RenderCopy(renderer, texture, &src, &dst);
		const int strips = 16;
		for (int i = 0; i < strips; i++)
		{
			int sy0 = h - h * fadeH / artH + h * fadeH / artH * i / strips;
			int sy1 = h - h * fadeH / artH + h * fadeH / artH * (i + 1) / strips;
			int dy0 = inner.y + artH - fadeH + fadeH * i / strips, dy1 = inner.y + artH - fadeH + fadeH * (i + 1) / strips;
			if (sy1 <= sy0 || dy1 <= dy0)
				continue;
			SDL_Rect fs{0, sy0, w, sy1 - sy0}, fd{inner.x, dy0, inner.w, dy1 - dy0};
			SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(255 * (strips - i) / (strips + 1)));
			SDL_RenderCopy(renderer, texture, &fs, &fd);
		}
		SDL_SetTextureAlphaMod(texture, 255);
		y = static_cast<float>(inner.y + artH - fadeH / 5); // the score box just overlaps the fade, as on Space Cadet
	}

	auto draw = ImGui::GetForegroundDrawList();
	auto font = ImGui::GetFont();
	float small = std::max(12.0f, panel.h * 0.03f), big = small * 1.9f;
	auto ballNumber = std::max(1, table->MaxBallCount - table->BallCount + 1);
	auto message = live_message();
	int boxEdge = std::max(3, edge - 1);
	int pad = std::max(4, inner.w / 28);
	SDL_Rect boxArea{inner.x + pad, 0, inner.w - 2 * pad, 0}; // boxes span this width

	const ImU32 grey = IM_COL32(200, 196, 186, 255), gold = IM_COL32(255, 210, 80, 255),
	            cyan = IM_COL32(120, 220, 255, 255), dim = IM_COL32(150, 140, 120, 255);
	auto line = [&](const std::string& text, float size, ImU32 color, float wrap = 0.0f)
	{
		auto textSize = font->CalcTextSizeA(size, FLT_MAX, wrap, text.c_str());
		draw->AddText(font, size, ImVec2(panel.x + (panel.w - textSize.x) / 2, y), color, text.c_str(), nullptr, wrap);
		y += textSize.y * 1.15f;
	};

	// Score box: the dot-matrix display, or the score in plain text
	if (options::Options.DotMatrix)
	{
		Dmd dmd;
		if (message)
		{
			auto lines = Dmd::wrap(message, 21);
			if (lines.size() == 1 && Dmd::width(lines[0], 2) <= DmdW)
				dmd.centred(lines[0], 9, 2);
			else
			{
				if (lines.size() > 4)
					lines.resize(4);
				int top = (DmdH - static_cast<int>(lines.size()) * 8 + 1) / 2;
				for (size_t i = 0; i < lines.size(); i++)
					dmd.centred(lines[i], top + static_cast<int>(i) * 8, 1);
			}
		}
		else
		{
			auto score = with_commas(player_score(table, table->CurrentPlayer));
			int scale = Dmd::width(score, 3) <= DmdW ? 3 : 2;
			dmd.centred(score, scale == 3 ? 1 : 4, scale);
			dmd.centred("PLAYER " + std::to_string(table->CurrentPlayer + 1) + "    BALL " + std::to_string(ballNumber),
			            DmdH - 8, 1, 150);
		}
		// The display wants 128 dots at 2 pixels or more: in a narrow panel the box uses the whole width inside the
		// frame and a thinner border
		int k = Dmd::pitch(static_cast<float>(boxArea.w - 2 * boxEdge - 4));
		int dmdEdge = boxEdge, gap = k;
		SDL_Rect box{boxArea.x, static_cast<int>(y), boxArea.w, 0};
		if (DmdW * k + 2 * (dmdEdge + gap) > box.w)
		{
			box.x = inner.x;
			box.w = inner.w;
			dmdEdge = box.w >= DmdW * k + 6 ? 3 : 2;
			gap = std::max(0, (box.w - DmdW * k - 2 * dmdEdge) / 2);
		}
		box.h = DmdH * k + 2 * (dmdEdge + std::min(gap, k));
		inset_box(box, dmdEdge, 255);
		dmd.draw(static_cast<float>(box.x), static_cast<float>(box.y + dmdEdge + std::min(gap, k)),
		         static_cast<float>(box.w));
		y = static_cast<float>(box.y + box.h) + small * 0.6f;
	}
	else
	{
		SDL_Rect box{boxArea.x, static_cast<int>(y), boxArea.w, static_cast<int>(small * 1.3f + big * 1.2f + small * 0.6f)};
		inset_box(box, boxEdge, 235);
		y += boxEdge + small * 0.2f;
		line("Player " + std::to_string(table->CurrentPlayer + 1) + "    Ball " + std::to_string(ballNumber), small, grey);
		line(with_commas(player_score(table, table->CurrentPlayer)), big, gold);
		y = static_cast<float>(box.y + box.h) + small * 0.6f;
	}

	// High score box at the bottom, message box between the two
	auto& best = high_score::highscore_table[0];
	int bestH = static_cast<int>(small * 2.6f) + 2 * boxEdge;
	SDL_Rect bestBox{boxArea.x, inner.y + inner.h - pad - bestH, boxArea.w, bestH};
	SDL_Rect infoBox{boxArea.x, static_cast<int>(y), boxArea.w, bestBox.y - static_cast<int>(small * 0.6f) - static_cast<int>(y)};
	if (infoBox.h > small * 2)
	{
		inset_box(infoBox, boxEdge);
		draw->PushClipRect(ImVec2(static_cast<float>(infoBox.x + boxEdge), static_cast<float>(infoBox.y + boxEdge)),
		                   ImVec2(static_cast<float>(infoBox.x + infoBox.w - boxEdge),
		                          static_cast<float>(infoBox.y + infoBox.h - boxEdge)), true);
		y = static_cast<float>(infoBox.y + boxEdge) + small * 0.5f;
		float wrap = static_cast<float>(infoBox.w - 2 * boxEdge) - small;
		if (!options::Options.DotMatrix && message)
			line(message, small, cyan, wrap);
		// Every player's score in a multi-player game, the one playing marked
		if (table->PlayerCount > 1)
			for (int player = 0; player < table->PlayerCount; player++)
			{
				auto current = player == table->CurrentPlayer;
				line((current ? "> " : "") + std::string("Player ") + std::to_string(player + 1) + "   " +
				     with_commas(player_score(table, player)), small, current ? gold : grey);
			}
		if (table->ExtraBalls > 0)
			line("EXTRA BALL LIT", small, IM_COL32(120, 255, 140, 255));
		// What the table rules are doing (bonus multiplier, lit pockets, Captain Hero's pops and henchmen, ...)
		for (auto& status : ft2_control::StatusLines())
			line(status, small, cyan, wrap);
		draw->PopClipRect();
	}
	inset_box(bestBox, boxEdge);
	y = static_cast<float>(bestBox.y + boxEdge) + small * 0.25f;
	line("HIGH SCORE", small * 0.85f, dim);
	line(best.Score > 0 ? with_commas(best.Score) + "  " + best.Name : std::string("-"), small, grey);

	bevel(panel, edge); // the panel's own frame last, over the art's edges
}

void side_panel::RefreshTableBoxes()
{
	auto table = pb::MainTable;
	if (!table)
		return;
	for (auto box : {table->CurScoreStruct, table->ScoreBallcount, table->ScorePlayerNumber1})
		if (box)
		{
			box->DirtyFlag = true;
			score::update(box);
		}
	// Message boxes on FT2 tables are drawn fresh every frame (TTextBox::DrawImGui), so they follow by themselves
}
