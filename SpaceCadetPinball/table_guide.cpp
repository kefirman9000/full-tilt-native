#include "pch.h"
#include "table_guide.h"

#include "ft2_control.h"
#include "high_score.h"
#include "imgui.h"
#include "options.h"
#include "pb.h"
#include "TPinballTable.h"

bool table_guide::ShowWindow = false;

namespace
{
	Uint32 GameStartTicks = 0;

	std::string table_key()
	{
		std::string key = pb::DatFileName;
		auto slash = key.find_last_of("/\\");
		if (slash != std::string::npos)
			key = key.substr(slash + 1);
		for (auto& c : key)
			c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
		return key;
	}

	std::string stat_key(const char* field)
	{
		return table_key() + " Stats." + field;
	}

	long long get_ll(const char* field)
	{
		auto text = options::GetSetting(stat_key(field), "0");
		return atoll(text.c_str());
	}

	void set_ll(const char* field, long long value)
	{
		options::SetSetting(stat_key(field), std::to_string(value));
	}

	std::string with_commas(long long value)
	{
		auto digits = std::to_string(value);
		for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
			digits.insert(static_cast<size_t>(i), ",");
		return digits;
	}

	const char* GenericRules =
		"Every part scores when the ball hits it (the table's real values where they are known).\n"
		"Pockets and holes send the ball back out after a moment.\n"
		"\n"
		"Enhanced Rules (Options > Table Rules, on by default):\n"
		"- Sets: lanes, targets and drop targets that share a name form a set. Hit every part of a set for "
		"10,000 per part times the bonus multiplier. Each set raises the multiplier by one, up to 5x. "
		"Drop target banks come back up.\n"
		"- End-of-ball bonus: 1,000 for every scoring hit this ball, times the multiplier.\n"
		"- Extra ball once per player per game at 1,500,000.\n"
		"- Ball save: a ball lost within a few seconds of the launch comes back (Options > Table Rules).\n"
		"\n"
		"Practice mode (Options > Table Rules) gives every lost ball back. Practice games do not count for "
		"high scores or statistics.";

	struct TableRules
	{
		const char* Table;
		const char* Title;
		const char* Text;
		bool Generic; // also uses the generic rules above
	};

	const TableRules Rules[] =
	{
		{
			"HERO.DAT", "Captain Hero",
			"Rules recovered from the original table program.\n"
			"\n"
			"Villain battles: shoot the scoop when no battle is on. The display offers a villain: left flipper for "
			"the next one, right flipper to fight (or wait 20 seconds). Starting a battle pays 100,000; beating the "
			"villain pays the jackpot, which then goes back to 500,000. Losing the ball loses the battle.\n"
			"- Quake: knock down the 9 drop targets (75,000 each during the battle).\n"
			"- Mad Scientist: shoot the scoop again.\n"
			"- Mystman: a henchman comes out; hit him 5 times (10,000, 20,000, 30,000, 40,000).\n"
			"- Slime Man: hit the pop bumpers 8 times.\n"
			"- Mr. Pling: hit Mr. Pling's target 8 times.\n"
			"- Evil Twin: blow up Krypton-@ with 5 shots at its four drop targets, its kickout, its scoop or the "
			"downtown ramp (200,000 each).\n"
			"Beat all six and they all come back.\n"
			"\n"
			"- Skill shot: pull the plunger back a little (about a quarter of a second) so the ball only just "
			"reaches the top of the shooter lane: 500,000 and the jackpot grows by 500,000, or 500,000 and the "
			"forcefields (the ball saver between the flippers opens until it saves a ball), or 1,000,000, or once "
			"a game 500,000 and an extra ball. Too soft and the ball comes back down: just plunge again.\n"
			"- Ball saver: the hole between the flippers is open for the ball save time after each launch "
			"(Options > Table Rules) and saves one ball.\n"
			"- Pop bumpers: 1,000 a hit, 1,000 more every 8 hits, up to 5,000. Back to 1,000 for each new ball.\n"
			"- Mid-table pockets open for 20 seconds when you complete a set: 50,000 each.\n"
			"- Slingshots and spinner 750. Lanes 5,000 to 20,000, lower lanes 3,500.\n"
			"- Targets and drop targets 5,000 to 100,000 a hit; scoop holes 25,000 to 70,000. Downtown ramp 25,000.\n"
			"Not in yet: the battles' time limits and the final stage.",
			true
		},
		{
			"ALIEN.DAT", "Alien Daze",
			"Every part scores its real value from the original table program. The table's own missions are not "
			"ported yet.",
			true
		},
		{
			"MAD.DAT", "Mad Scientist",
			"Every part scores its real value from the original table program; the beaker bumpers build up from "
			"1,000 by 1,000 every 10 hits, up to 10,000. The table's own missions are not ported yet.",
			true
		},
		{
			"DRAGON.DAT", "Dragon's Keep",
			"Real point values from the original table program; the rebounders and spot targets use their "
			"original rules. The rest of the table's rules are being ported.",
			true
		},
		{
			"PIRATES.DAT", "Skulduggery",
			"Real point values from the original table program. The table's own rules are not ported yet.",
			true
		},
		{
			"CADET.DAT", "Space Cadet",
			"Space Cadet plays with its full original rules: complete missions to rise in rank from Cadet to "
			"Fleet Admiral. The launch ramp, hyperspace, wormholes, fuel lights, gravity well and black hole play "
			"as in Windows' 3D Pinball, and the table's message box tells you which mission is running and what "
			"to shoot.",
			false
		},
	};

	// Wrapped at spaces only (ImGui's own wrapping also breaks after commas, which splits numbers like 50,000)
	void wrapped(const char* text)
	{
		auto width = ImGui::GetContentRegionAvail().x;
		std::string paragraph;
		auto flush_paragraph = [&]()
		{
			std::string line, word;
			auto emit = [&]() { ImGui::TextUnformatted(line.c_str()); line.clear(); };
			size_t i = 0;
			while (i <= paragraph.size())
			{
				auto end = paragraph.find(' ', i);
				if (end == std::string::npos)
					end = paragraph.size();
				word = paragraph.substr(i, end - i);
				auto candidate = line.empty() ? word : line + " " + word;
				if (!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > width)
				{
					emit();
					line = word;
				}
				else
					line = candidate;
				i = end + 1;
			}
			emit();
		};
		for (auto c = text; ; c++)
		{
			if (*c == '\n' || !*c)
			{
				flush_paragraph();
				paragraph.clear();
				if (!*c)
					break;
			}
			else
				paragraph += *c;
		}
	}

	void render_rules()
	{
		auto key = table_key();
		const TableRules* rules = nullptr;
		for (auto& entry : Rules)
			if (key == entry.Table)
				rules = &entry;
		if (rules)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.3f, 1.0f), "%s", rules->Title);
			wrapped(rules->Text);
		}
		if (!rules || rules->Generic)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "Full Tilt Native rules");
			wrapped(GenericRules);
		}
	}

	void render_stats()
	{
		auto games = get_ll("Games");
		if (games <= 0)
		{
			ImGui::TextUnformatted("No games finished on this table yet.");
		}
		else
		{
			auto seconds = get_ll("Seconds");
			ImGui::Text("Games played      %s", with_commas(games).c_str());
			ImGui::Text("Best game         %s", with_commas(get_ll("Best")).c_str());
			ImGui::Text("Average game      %s", with_commas(get_ll("Total") / games).c_str());
			ImGui::Text("Last game         %s", with_commas(get_ll("Last")).c_str());
			ImGui::Text("Time played       %lld h %02lld min", seconds / 3600, seconds / 60 % 60);
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextUnformatted("High scores");
		for (auto& entry : high_score::highscore_table)
			if (entry.Score > 0)
				ImGui::Text("%14s  %s", with_commas(entry.Score).c_str(), entry.Name);
		ImGui::Spacing();
		ImGui::TextDisabled("Practice and demo games are not counted.");
	}
}

void table_guide::GameStarted()
{
	GameStartTicks = SDL_GetTicks();
}

void table_guide::GameOver(TPinballTable* table)
{
	if (!table || pb::IsDemo() || ft2_control::PracticeGame || !GameStartTicks)
		return;
	auto seconds = static_cast<long long>((SDL_GetTicks() - GameStartTicks) / 1000);
	GameStartTicks = 0;
	set_ll("Seconds", get_ll("Seconds") + seconds);
	for (int player = 0; player < table->PlayerCount; player++)
	{
		auto& saved = table->PlayerScores[player];
		long long score = player == table->CurrentPlayer
			                  ? static_cast<long long>(table->CurScoreE9) * 1000000000LL + table->CurScore
			                  : static_cast<long long>(saved.ScoreE9Part) * 1000000000LL + saved.Score;
		set_ll("Games", get_ll("Games") + 1);
		set_ll("Total", get_ll("Total") + score);
		set_ll("Last", score);
		if (score > get_ll("Best"))
			set_ll("Best", score);
	}
}

void table_guide::Render()
{
	if (!ShowWindow)
		return;
	ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Table Guide", &ShowWindow))
	{
		if (ImGui::BeginTabBar("guide"))
		{
			if (ImGui::BeginTabItem("Rules"))
			{
				render_rules();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Statistics"))
			{
				render_stats();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}
