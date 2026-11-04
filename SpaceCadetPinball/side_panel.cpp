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
	// ---- Table art: the table's title screen (see table_art) ----
	SDL_Texture* art()
	{
		static bool tried = false;
		static SDL_Texture* texture = nullptr;
		if (!tried)
		{
			tried = true;
			if (auto surface = table_art::Load())
			{
				texture = SDL_CreateTextureFromSurface(winmain::Renderer, surface);
				SDL_FreeSurface(surface);
				if (texture)
					SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
			}
		}
		return texture;
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
		// then a dark gap; from 4 pixels a dot the corners are softened. Returns the height used.
		float draw(float x, float y, float w)
		{
			int k = std::max(2, static_cast<int>(w / DmdW));
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
			int dx0 = static_cast<int>(x + (w - dw) / 2), dy0 = static_cast<int>(y);
			SDL_Rect frame{dx0 - k * 2, dy0 - k * 2, dw + k * 4, dh + k * 4};
			SDL_SetRenderDrawColor(winmain::Renderer, 70, 45, 20, 255);
			SDL_RenderFillRect(winmain::Renderer, &frame);
			SDL_Rect inner{frame.x + 1, frame.y + 1, frame.w - 2, frame.h - 2};
			SDL_SetRenderDrawColor(winmain::Renderer, 12, 6, 2, 255);
			SDL_RenderFillRect(winmain::Renderer, &inner);
			SDL_Rect dst{dx0, dy0, dw, dh};
			SDL_RenderCopy(winmain::Renderer, texture, nullptr, &dst);
			return static_cast<float>(dh + k * 2);
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
	SDL_SetRenderDrawBlendMode(winmain::Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(winmain::Renderer, 8, 8, 14, 235);
	SDL_RenderFillRect(winmain::Renderer, &panel);

	float margin = panel.w * 0.05f;
	float y = static_cast<float>(panel.y) + panel.h * 0.025f;
	if (auto texture = art())
	{
		int w, h;
		SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
		float scale = std::min((panel.w - margin * 2) / w, panel.h * 0.33f / h);
		SDL_Rect dst{panel.x + static_cast<int>((panel.w - w * scale) / 2), static_cast<int>(y),
		             static_cast<int>(w * scale), static_cast<int>(h * scale)};
		SDL_SetTextureColorMod(texture, 255, 255, 255);
		SDL_RenderCopy(winmain::Renderer, texture, nullptr, &dst);
		y += dst.h + panel.h * 0.03f;
	}

	auto draw = ImGui::GetForegroundDrawList();
	auto font = ImGui::GetFont();
	float small = std::max(12.0f, panel.h * 0.032f), big = small * 1.9f;
	auto ballNumber = std::max(1, table->MaxBallCount - table->BallCount + 1);
	auto message = live_message();

	// Dot-matrix display (or plain text): the score, or the message while one is showing
	float dmdW = panel.w - margin * 2;
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
		y += dmd.draw(panel.x + margin, y, dmdW) + small * 1.0f;
	}

	auto line = [&](const std::string& text, float size, ImU32 color, float wrap = 0.0f)
	{
		auto textSize = font->CalcTextSizeA(size, FLT_MAX, wrap, text.c_str());
		draw->AddText(font, size, ImVec2(panel.x + (panel.w - textSize.x) / 2, y), color, text.c_str(), nullptr, wrap);
		y += textSize.y * 1.15f;
	};
	const ImU32 grey = IM_COL32(190, 190, 200, 255), gold = IM_COL32(255, 210, 80, 255),
	            cyan = IM_COL32(120, 220, 255, 255), dim = IM_COL32(130, 130, 145, 255);

	if (!options::Options.DotMatrix)
	{
		line("Player " + std::to_string(table->CurrentPlayer + 1), small, grey);
		line(with_commas(player_score(table, table->CurrentPlayer)), big, gold);
		line("Ball " + std::to_string(ballNumber), small, grey);
		if (message)
			line(message, small, cyan, panel.w - margin * 2);
		y += small * 0.5f;
	}

	// Every player's score in a multi-player game, the one playing marked
	if (table->PlayerCount > 1)
	{
		for (int player = 0; player < table->PlayerCount; player++)
		{
			auto current = player == table->CurrentPlayer;
			line((current ? "> " : "") + std::string("Player ") + std::to_string(player + 1) + "   " +
			     with_commas(player_score(table, player)), small, current ? gold : grey);
		}
		y += small * 0.5f;
	}

	if (table->ExtraBalls > 0)
		line("EXTRA BALL LIT", small, IM_COL32(120, 255, 140, 255));

	// What the table rules are doing (bonus multiplier, lit pockets, Captain Hero's pops and henchmen, ...)
	for (auto& status : ft2_control::StatusLines())
		line(status, small, cyan, panel.w - margin * 2);

	// The table's best score
	auto& best = high_score::highscore_table[0];
	if (best.Score > 0)
	{
		y = std::max(y, static_cast<float>(panel.y + panel.h) - small * 3.0f);
		line("High score", small * 0.85f, dim);
		line(with_commas(best.Score) + "  " + best.Name, small, grey);
	}
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
