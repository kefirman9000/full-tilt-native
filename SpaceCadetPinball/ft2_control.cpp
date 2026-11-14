#include "pch.h"
#include "ft2_control.h"
#include "ft2_scores.h"
#include "ft2_sounds.h"
#include "ft2_animation.h"
#include "ft2_alien.h"

#include "pb.h"
#include "TBall.h"
#include "table_art.h"
#include "loader.h"
#include "options.h"
#include "timer.h"
#include "TLightGroup.h"
#include "translations.h"
#include "TBumper.h"
#include "TDrain.h"
#include "TFlagSpinner.h"
#include "TGate.h"
#include "TBlocker.h"
#include "ft_log.h"
#include "TTableLayer.h"
#include "THole.h"
#include "TKickback.h"
#include "TKickout.h"
#include "TLightRollover.h"
#include "TOneway.h"
#include "TPinballTable.h"
#include "TPlunger.h"
#include "TPopupTarget.h"
#include "TRamp.h"
#include "TRollover.h"
#include "TSink.h"
#include "TSoloTarget.h"
#include "TSound.h"
#include "TTextBox.h"
#include "TTripwire.h"

namespace
{
	TPinballTable* table()
	{
		return pb::MainTable;
	}

	std::string table_key()
	{
		std::string key = pb::DatFileName;
		auto slash = key.find_last_of("/\\");
		if (slash != std::string::npos)
			key = key.substr(slash + 1);
		std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		return key;
	}

	// Real value from the table's original rule table when known (Alien Daze), else a placeholder per component type
	int recovered_points(TPinballComponent* caller)
	{
		if (!caller->GroupName)
			return -1;
		auto table = table_key();
		for (auto& entry : ft2_scores)
			if (table == entry.Table && strcmp(caller->GroupName, entry.Component) == 0)
				return entry.Scores.empty() ? -1 : entry.Scores[0];
		return -1;
	}

	// The table's own sound for this component (its "waveNN" sound object), as the original rules play it
	void play_component_sound(TPinballComponent* caller)
	{
		static std::map<const TPinballComponent*, TSound*> cache;
		static TPinballTable* cacheTable = nullptr;
		auto t = table();
		if (cacheTable != t)
		{
			cache.clear();
			cacheTable = t;
		}
		auto found = cache.find(caller);
		if (found == cache.end())
		{
			TSound* sound = nullptr;
			if (caller->GroupName)
			{
				auto key = table_key();
				for (auto& entry : ft2_sounds)
					if (key == entry.Table && strcmp(caller->GroupName, entry.Component) == 0)
					{
						for (auto component : t->ComponentList)
							if (component->GroupName && strcmp(component->GroupName, entry.Wave) == 0)
								sound = dynamic_cast<TSound*>(component);
						break;
					}
			}
			found = cache.emplace(caller, sound).first;
		}
		if (found->second)
			found->second->Play(caller, caller->GroupName);
	}

	int points_for(TPinballComponent* caller)
	{
		auto recovered = recovered_points(caller);
		if (recovered >= 0)
			return recovered;
		if (dynamic_cast<TBumper*>(caller)) return 1000;
		if (dynamic_cast<TPopupTarget*>(caller)) return 2500;
		if (dynamic_cast<TSoloTarget*>(caller)) return 2500;
		if (dynamic_cast<TLightRollover*>(caller)) return 1000;
		if (dynamic_cast<TRollover*>(caller)) return 1000;
		if (dynamic_cast<TFlagSpinner*>(caller)) return 250;
		if (dynamic_cast<TTripwire*>(caller)) return 2000;
		if (dynamic_cast<TKickout*>(caller)) return 5000;
		if (dynamic_cast<TSink*>(caller)) return 5000;
		if (dynamic_cast<THole*>(caller)) return 5000;
		if (dynamic_cast<TRamp*>(caller)) return 10000;
		if (dynamic_cast<TKickback*>(caller)) return 500;
		if (dynamic_cast<TGate*>(caller)) return 500;
		if (dynamic_cast<TOneway*>(caller)) return 500;
		return 0;
	}


	// ---- Enhanced rules (Options > Table Rules): what most real tables do, built from the table's own parts ----
	bool enhanced()
	{
		return options::Options.EnhancedRules;
	}

	std::string with_commas(long long value)
	{
		auto digits = std::to_string(value);
		for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
			digits.insert(i, ",");
		return digits;
	}

	std::string upper_copy(std::string text)
	{
		for (auto& c : text)
			c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
		return text;
	}

	void show(const std::string& text, float seconds = 2.0f)
	{
		if (pb::InfoTextBox)
			pb::InfoTextBox->Display(text.c_str(), seconds);
	}

	// A set is the parts of one kind that share a name apart from their number (a_drop05..07, a_roll01..05)
	struct PartSet
	{
		std::vector<TPinballComponent*> Parts;
		std::vector<TPinballComponent*> Hit;
		bool DropBank = false;
	};

	struct RuleState
	{
		TPinballTable* Table = nullptr;
		std::vector<PartSet> Sets;
		std::map<const TPinballComponent*, int> SetOf;
		int BonusHits = 0;
		int Multiplier = 1;
		bool BallSaveUsed = false;
		int LaunchTick = -1000000;
		bool ExtraBallGiven[4]{};
		int AttractTimer = 0;
		int FlashTimer = 0;
		// Pockets with a strong pull in the open playfield (Captain Hero's mid-table magnets): the original rules
		// only switch them on during modes, so here they stay closed and open as a reward
		std::vector<TKickout*> ModePockets;
		int PocketTimer = 0;
		// Bumpers that only come out in modes (Captain Hero's henchmen): one comes out per completed set or bank,
		// and is knocked out by HenchmanHits hits
		std::vector<TBumper*> Henchmen;
		std::map<const TBumper*, int> HenchmanHits;
		// Captain Hero (rules recovered from hero.exe): pop bumper value, skill shot
		int PopValue = 1000;
		int PopHits = 0;
		bool SkillShotLive = false;
		bool SkillShotExtraBall = false;
		bool AwaitingLaunch = false; // a ball sits in the shooter lane
		// Captain Hero's ball saver (s_sink30, the hole between the flippers): the original keeps it shut and opens
		// it only while a save is lit (hero.exe 0x404eec / drain rule 0x4070bc). Here it is open during the ball
		// save time after a launch, and saves one ball.
		TSink* SaveSink = nullptr;
		int SaveSinkTimer = 0;
		TSink* Scoop = nullptr; // Captain Hero's battle scoop (s_sink01)
	} rs;

	// Captain Hero's henchmen take five hits: 10,000 x the hit number for hits 1-4 (hero.exe 0x402668), the fifth
	// knocks him out
	const int HenchmanHitsNeeded = 5;

	bool is_hero()
	{
		return table_key() == "HERO.DAT";
	}

	bool named(TPinballComponent* part, std::initializer_list<const char*> names)
	{
		if (!part->GroupName)
			return false;
		for (auto name : names)
			if (strcmp(part->GroupName, name) == 0)
				return true;
		return false;
	}

	std::string set_key(TPinballComponent* part)
	{
		std::string name = part->GroupName ? part->GroupName : "";
		while (!name.empty() && (isdigit(static_cast<unsigned char>(name.back())) || name.back() == '_'))
			name.pop_back();
		const char* kind = dynamic_cast<TPopupTarget*>(part) ? "drop" : dynamic_cast<TSoloTarget*>(part) ? "target" : "lane";
		return name.empty() ? "" : std::string(kind) + ":" + name;
	}


	void set_pockets(bool open)
	{
		for (auto pocket : rs.ModePockets)
		{
			pocket->NotSomeFlag = open ? 0 : 1; // with the flag set a kickout stays off after it ejects
			if (open)
				pocket->ActiveFlag = 1;
			else if (!pocket->BallCaputeredFlag)
				pocket->ActiveFlag = 0;
		}
	}

	void set_save_sink(bool open)
	{
		if (!rs.SaveSink)
			return;
		rs.SaveSink->ActiveFlag = open ? 1 : 0;
		if (!open && rs.SaveSinkTimer)
		{
			timer::kill(rs.SaveSinkTimer);
			rs.SaveSinkTimer = 0;
		}
	}

	void close_save_sink(int, void*)
	{
		rs.SaveSinkTimer = 0;
		set_save_sink(false);
	}

	void close_pockets(int, void*)
	{
		rs.PocketTimer = 0;
		set_pockets(false);
	}

	void open_pockets(float seconds)
	{
		if (rs.ModePockets.empty())
			return;
		set_pockets(true);
		if (rs.PocketTimer)
			timer::kill(rs.PocketTimer);
		rs.PocketTimer = timer::set(seconds, nullptr, close_pockets);
	}

	void build_sets()
	{
		auto t = table();
		if (rs.Table == t)
			return;
		rs = RuleState{};
		rs.Table = t;
		std::map<std::string, std::vector<TPinballComponent*>> groups;
		for (auto part : t->ComponentList)
		{
			auto drop = dynamic_cast<TPopupTarget*>(part);
			if (drop && drop->HiddenUntilMode)
				continue; // shown by a table mode, not a normal bank
			if (!drop && !dynamic_cast<TSoloTarget*>(part) && !dynamic_cast<TRollover*>(part))
				continue;
			auto key = set_key(part);
			if (!key.empty())
				groups[key].push_back(part);
		}
		for (auto part : t->ComponentList)
		{
			auto pocket = dynamic_cast<TKickout*>(part);
			if (!pocket || pocket->FieldMult < 300.0f)
				continue;
			auto reach = loader::query_float_attribute(pocket->GroupIndex, 0, 306);
			if (reach && *reach >= 0.9f)
				rs.ModePockets.push_back(pocket);
		}
		for (auto part : t->ComponentList)
		{
			// Pockets with no picture and no ball height in the open playfield (Captain Hero's s_kout06..10) are
			// mode targets too: in normal play they only trap the ball over and over
			auto pocket = dynamic_cast<TKickout*>(part);
			if (pocket && !loader::query_float_attribute(pocket->GroupIndex, 0, 408) &&
				std::find(rs.ModePockets.begin(), rs.ModePockets.end(), pocket) == rs.ModePockets.end())
				rs.ModePockets.push_back(pocket);
		}
		for (auto part : t->ComponentList)
		{
			auto bumper = dynamic_cast<TBumper*>(part);
			if (bumper && bumper->HiddenUntilMode)
				rs.Henchmen.push_back(bumper);
			if (t->Plunger && named(part, {"ss_trigger"}))
				t->Plunger->SkillShotLow = 0.26f; // a tap falls short and rolls back, as on the original table
			if (is_hero() && named(part, {"s_sink30"}))
				rs.SaveSink = dynamic_cast<TSink*>(part);
			if (is_hero() && named(part, {"s_sink01"}))
				rs.Scoop = dynamic_cast<TSink*>(part);
		}
		set_pockets(false);
		set_save_sink(false);
		for (auto& group : groups)
		{
			if (group.second.size() < 2 || group.second.size() > 10)
				continue;
			PartSet set;
			set.Parts = group.second;
			set.DropBank = group.first.compare(0, 5, "drop:") == 0;
			for (auto part : set.Parts)
				rs.SetOf[part] = static_cast<int>(rs.Sets.size());
			rs.Sets.push_back(set);
		}
	}

	void stop_flash(int, void*)
	{
		rs.FlashTimer = 0;
		if (!rs.AttractTimer && table()->LightGroup)
			table()->LightGroup->Message(MessageCode::TLightGroupReset, 0.0);
	}

	void light_show(float seconds)
	{
		auto t = table();
		if (!t->LightGroup || rs.AttractTimer)
			return;
		t->LightGroup->Message(MessageCode::TLightGroupLightShowAnimation, 0.15f);
		if (rs.FlashTimer)
			timer::kill(rs.FlashTimer);
		rs.FlashTimer = timer::set(seconds, nullptr, stop_flash);
	}

	void attract_tick(int, void*)
	{
		rs.AttractTimer = 0;
		auto t = table();
		if (!t || !t->LightGroup)
			return;
		t->LightGroup->Message(MessageCode::TLightGroupLightShowAnimation, 0.3f);
		rs.AttractTimer = timer::set(3.0f, nullptr, attract_tick);
	}

	void stop_attract()
	{
		if (rs.AttractTimer)
		{
			timer::kill(rs.AttractTimer);
			rs.AttractTimer = 0;
			if (table()->LightGroup)
				table()->LightGroup->Message(MessageCode::TLightGroupReset, 0.0);
		}
	}

	void reset_bank(int, void* caller)
	{
		auto index = static_cast<int>(reinterpret_cast<intptr_t>(caller));
		if (index < 0 || index >= static_cast<int>(rs.Sets.size()))
			return;
		for (auto part : rs.Sets[index].Parts)
			part->Message(MessageCode::TPopupTargetEnable, 0.0);
	}

	long long current_score()
	{
		auto t = table();
		return static_cast<long long>(t->CurScoreE9) * 1000000000LL + t->CurScore;
	}

	void add_points(int points)
	{
		auto t = table();
		t->AddScore(points);
		if (!enhanced())
			return;
		// One extra ball per player per game for reaching the milestone
		auto player = t->CurrentPlayer;
		const long long milestone = 1500000;
		if (player >= 0 && player < 4 && !rs.ExtraBallGiven[player] && current_score() >= milestone)
		{
			rs.ExtraBallGiven[player] = true;
			t->ExtraBalls++;
			show("EXTRA BALL!", 3.0f);
			light_show(2.5f);
		}
	}

	// One hidden henchman comes out onto the table (picked at random among those not out yet)
	bool bring_out_henchman()
	{
		std::vector<TBumper*> hidden;
		for (auto henchman : rs.Henchmen)
			if (!henchman->Shown)
				hidden.push_back(henchman);
		if (hidden.empty())
			return false;
		auto henchman = hidden[rand() % hidden.size()];
		henchman->SetShown(true);
		if (pb::DevDebug)
			printf("FT2DBG henchman out: %s\n", henchman->GroupName ? henchman->GroupName : "?");
		rs.HenchmanHits[henchman] = 0;
		show("ENEMY ON THE LOOSE  HIT IT " + std::to_string(HenchmanHitsNeeded) + " TIMES", 2.5f);
		return true;
	}

	void win_battle();
	bool hero_mystman_battle();

	void henchman_hit(TBumper* henchman)
	{
		if (!henchman->Shown)
			return;
		auto hits = ++rs.HenchmanHits[henchman];
		if (hits < HenchmanHitsNeeded)
		{
			add_points(10000 * hits);
			rs.BonusHits++;
			show("HIT " + std::to_string(hits) + " OF " + std::to_string(HenchmanHitsNeeded) + "  " +
			     with_commas(10000 * hits), 1.5f);
			return;
		}
		henchman->SetShown(false);
		rs.HenchmanHits[henchman] = 0;
		if (pb::DevDebug)
			printf("FT2DBG henchman knocked out: %s\n", henchman->GroupName ? henchman->GroupName : "?");
		// The fifth hit wins Mystman's battle (Captain Hero: the henchmen only come out in it)
		if (hero_mystman_battle())
		{
			win_battle();
			return;
		}
		auto award = 10000 * HenchmanHitsNeeded * rs.Multiplier;
		add_points(award);
		show("ENEMY KNOCKED OUT  " + with_commas(award), 2.5f);
		light_show(1.5f);
	}

	// Pop bumpers that build up (hero.exe 0x4022f8, mad.exe 0x42d484): worth Start, Step more every Every hits, up to
	// Max; back to Start for each new ball
	struct PopRule
	{
		const char* Table;
		const char* Names[3];
		int Start, Step, Every, Max;
	};
	const PopRule PopRules[] =
	{
		{"HERO.DAT", {"a_bump01", "a_bump02", "a_bump03"}, 1000, 1000, 8, 5000},
		{"MAD.DAT", {"beaker_bumper01", "beaker_bumper02", "beaker_bumper03"}, 1000, 1000, 10, 10000},
	};

	const PopRule* pop_rule(TPinballComponent* caller)
	{
		auto key = table_key();
		for (auto& rule : PopRules)
			if (key == rule.Table && named(caller, {rule.Names[0], rule.Names[1], rule.Names[2]}))
				return &rule;
		return nullptr;
	}

	int pop_points(const PopRule& rule)
	{
		if (rs.PopValue < rule.Start)
			rs.PopValue = rule.Start;
		auto value = rs.PopValue;
		if (++rs.PopHits >= rule.Every)
		{
			rs.PopHits = 0;
			rs.PopValue = std::min(rule.Max, rs.PopValue + rule.Step);
			if (rs.PopValue > value)
				show("BUMPERS NOW " + with_commas(rs.PopValue), 1.5f);
		}
		return value;
	}

	// Captain Hero's skill shot (hero.exe 0x401da0): a launch that only just reaches the trigger at the top of the
	// shooter lane. The original pays when the ball crosses slower than 80; this engine's ball can arrive a little
	// faster for the softest launch that gets there, so the limit here is 100. Too soft and the ball rolls back to
	// the plunger (try again), just right is the skill shot, harder is a normal launch. Award: 7 in 10 500,000,
	// 2 in 10 1,000,000, 1 in 10 500,000 and an extra ball (once per game; after that 500,000).


	// ---- Captain Hero's villain battles (hero.exe: battle number 0x43d088, start 0x40787c, end 0x407e08) ----
	// Shoot the scoop (s_sink01, 0x406580) when no battle is on: it holds the ball and offers the villains not yet
	// beaten (left flipper: next, right flipper: accept, 20 s: taken as it is). A battle pays 100,000 to start and is
	// won by its own shots (from each villain's checks in the rules): 1 Quake the nine drop targets a_drop01..09,
	// 2 Mad Scientist the scoop again, 3 Mystman a henchman out after 4 s, hit 5 times, 4 Slime Man 8 pop bumper hits,
	// 5 Mr. Pling 8 hits on a_rebo04, 6 Evil Twin "Blow up Krypton-@": five shots at its targets (drop targets
	// a_drop18..21, s_kout01, s_sink14 and the downtown ramp), 200,000 each. Winning pays the jackpot (starts at and
	// goes back to 500,000; the skill shot raises it). Losing the ball loses the battle. Not ported yet: battle
	// timers (the original counts down on the display), Mr. Pling's path through s_ramp02, the seventh stage.
	const int VillainCount = 6;
	const char* const VillainNames[VillainCount] = {"Quake", "Mad Scientist", "Mystman", "Slime Man", "Mr. Pling",
	                                                 "Evil Twin"};
	const char* const VillainGoals[VillainCount] = {"knock down the 9 drop targets", "shoot the scoop again",
	                                                 "hit the henchman 5 times", "hit the pop bumpers 8 times",
	                                                 "hit Mr. Pling's target 8 times",
	                                                 "blow up Krypton-@ (5 shots)"};
	const int VillainGoal[VillainCount] = {9, 1, 5, 8, 8, 5};
	const int CaptureTexts[VillainCount] = {3069, 3071, 3073, 3075, 3077, 0};

	struct HeroBattles
	{
		int Battle = 0; // 0 none, else 1..6
		int Progress = 0;
		bool Defeated[VillainCount]{};
		bool Choosing = false;
		std::vector<int> Choices;
		size_t ChoiceIndex = 0;
		int ChooseTimer = 0;
		int HenchmanTimer = 0;
		bool QuakeDown[9]{};
		long long Jackpot = 500000;
		long long Captured = 0;
	} hb;

	bool hero_mystman_battle()
	{
		return is_hero() && hb.Battle == 3;
	}

	// A text of the table program (hero.exe), cleaned for display: its '>' is an apostrophe, '`' and '[' are
	// padding and icons
	std::string hero_text(int id, const std::string& fallback)
	{
		auto text = table_art::Text(id, fallback);
		std::string out;
		for (size_t i = 0; i < text.size(); i++)
		{
			auto c = text[i];
			if (c == '>' && i > 0 && isalpha(static_cast<unsigned char>(text[i - 1])))
				out += '\'';
			else if (c == '\n')
				out += ' ';
			else if (c != '`' && c != '[')
				out += c;
		}
		while (!out.empty() && out[0] == ' ')
			out.erase(0, 1);
		while (!out.empty() && out.back() == ' ')
			out.pop_back();
		return out;
	}

	std::string villain(int index)
	{
		return hero_text(3042 + index, VillainNames[index]);
	}

	void hide_henchmen()
	{
		if (hb.HenchmanTimer)
		{
			timer::kill(hb.HenchmanTimer);
			hb.HenchmanTimer = 0;
		}
		for (auto henchman : rs.Henchmen)
			if (henchman->Shown)
				henchman->SetShown(false);
		rs.HenchmanHits.clear();
	}

	void henchman_arrives(int, void*)
	{
		hb.HenchmanTimer = 0;
		if (hb.Battle == 3)
			bring_out_henchman();
	}

	void start_battle(int index)
	{
		if (pb::DevDebug)
			printf("FT2DBG battle start %d %s\n", index + 1, VillainNames[index]);
		hb.Battle = index + 1;
		hb.Progress = 0;
		for (auto& down : hb.QuakeDown)
			down = false;
		add_points(100000);
		show("BATTLE " + upper_copy(villain(index)) + "  " + upper_copy(VillainGoals[index]), 3.5f);
		light_show(1.5f);
		if (hb.Battle == 3)
			hb.HenchmanTimer = timer::set(4.0f, nullptr, henchman_arrives);
	}

	void show_choice()
	{
		auto text = hero_text(3063, "Battle %s? Right flipper to accept, Left flipper for next villain.");
		auto at = text.find("%s");
		auto name = villain(hb.Choices[hb.ChoiceIndex]);
		if (at != std::string::npos)
			text.replace(at, 2, name);
		else
			text = "Battle " + name + "?";
		if (pb::InfoTextBox)
			pb::InfoTextBox->Clear();
		show(text, 20.0f);
	}

	void accept_battle(int, void*)
	{
		if (!hb.Choosing)
			return;
		hb.Choosing = false;
		if (hb.ChooseTimer)
		{
			timer::kill(hb.ChooseTimer);
			hb.ChooseTimer = 0;
		}
		if (pb::InfoTextBox)
			pb::InfoTextBox->Clear();
		start_battle(hb.Choices[hb.ChoiceIndex]);
		if (rs.Scoop && !(pb::DevDebug && getenv("FT2_OFFER")))
			rs.Scoop->Message(MessageCode::TSinkResetTimer, 2.0f); // the ball comes back out
	}

	void offer_battle()
	{
		hb.Choices.clear();
		for (int i = 0; i < VillainCount; i++)
			if (!hb.Defeated[i])
				hb.Choices.push_back(i);
		if (hb.Choices.empty()) // every villain beaten: they all come back
		{
			for (auto& defeated : hb.Defeated)
				defeated = false;
			for (int i = 0; i < VillainCount; i++)
				hb.Choices.push_back(i);
		}
		hb.ChoiceIndex = 0;
		hb.Choosing = true;
		if (pb::DevDebug)
			printf("FT2DBG battle offer, %d villains\n", static_cast<int>(hb.Choices.size()));
		if (hb.Choices.size() == 1)
		{
			accept_battle(0, nullptr);
			return;
		}
		show_choice();
		hb.ChooseTimer = timer::set(20.0f, nullptr, accept_battle);
	}

	void win_battle()
	{
		auto index = hb.Battle - 1;
		if (pb::DevDebug)
			printf("FT2DBG battle won %d jackpot %lld\n", hb.Battle, hb.Jackpot);
		hb.Defeated[index] = true;
		hb.Captured++;
		hb.Battle = 0;
		hide_henchmen();
		auto award = hb.Jackpot;
		add_points(static_cast<int>(std::min<long long>(award, 2000000000)));
		auto fallback = std::string(VillainNames[index]) + " Captured, Jackpot Awarded!!!";
		auto text = CaptureTexts[index] ? hero_text(CaptureTexts[index], fallback) : fallback;
		show(upper_copy(text) + "  " + with_commas(award), 4.0f);
		light_show(3.0f);
		hb.Jackpot = 500000;
	}

	void battle_progress(int step = 1)
	{
		if (!hb.Battle)
			return;
		hb.Progress += step;
		auto index = hb.Battle - 1;
		if (pb::DevDebug)
			printf("FT2DBG battle %d progress %d/%d\n", hb.Battle, hb.Progress, VillainGoal[index]);
		if (hb.Progress >= VillainGoal[index])
		{
			win_battle();
			return;
		}
		show(upper_copy(villain(index)) + "  " + std::to_string(VillainGoal[index] - hb.Progress) + " TO GO", 1.5f);
	}

	void lose_battle()
	{
		if (hb.Choosing)
		{
			hb.Choosing = false;
			if (hb.ChooseTimer)
				timer::kill(hb.ChooseTimer);
			hb.ChooseTimer = 0;
		}
		if (!hb.Battle)
			return;
		if (pb::DevDebug)
			printf("FT2DBG battle lost %d\n", hb.Battle);
		show(upper_copy(villain(hb.Battle - 1)) + " GOT AWAY", 2.5f);
		hb.Battle = 0;
		hide_henchmen();
	}

	// Quake's drop targets: each target has two parts sharing one rule (hero.exe 0x403e00..0x403f10)
	int quake_target(TPinballComponent* part)
	{
		const char* pairs[9][2] = {{"a_drop01", "a_drop10"}, {"a_drop02", "a_drop23"}, {"a_drop03", "a_drop22"},
		                           {"a_drop04", "a_drop24"}, {"a_drop05", "a_drop26"}, {"a_drop06", "a_drop27"},
		                           {"a_drop07", "a_drop28"}, {"a_drop08", "a_drop47"}, {"a_drop09", "a_drop48"}};
		for (int i = 0; i < 9; i++)
			if (named(part, {pairs[i][0], pairs[i][1]}))
				return i;
		return -1;
	}

	// The battle rules for one hit; returns the points to score instead of the part's own, or -1 for its own
	int hero_battle_hit(TPinballComponent* caller)
	{
		switch (hb.Battle)
		{
		case 1:
			{
				auto target = quake_target(caller);
				if (target >= 0)
				{
					if (!hb.QuakeDown[target])
					{
						hb.QuakeDown[target] = true;
						battle_progress();
					}
					return 75000; // lit during Quake's battle (0x403aec)
				}
				break;
			}
		case 4:
			if (named(caller, {"a_bump01", "a_bump02", "a_bump03"}))
				battle_progress();
			break;
		case 5:
			if (named(caller, {"a_rebo04"}))
				battle_progress();
			break;
		case 6:
			if (named(caller, {"a_drop18", "a_drop19", "a_drop20", "a_drop21", "s_kout01", "s_sink14", "downtown_ramp"}))
			{
				add_points(200000); // a piece of Krypton-@ (0x406c14)
				battle_progress();
			}
			break;
		default:
			break;
		}
		return -1;
	}

	// Captain Hero's skill shot (hero.exe 0x401da0): a launch that only just reaches the trigger at the top of the
	// shooter lane. The original pays when the ball crosses slower than 80; this engine's ball can arrive a little
	// faster for the softest launch that gets there, so the limit here is 100. Too soft and the ball rolls back to
	// the plunger (try again), just right is the skill shot, harder is a normal launch. Award (a roll of 1-10):
	// 1-5 500,000 and the jackpot grows by 500,000 (up to 5,000,000), 6-7 500,000 and the forcefields (the ball
	// saver between the flippers stays open until it saves a ball), 8-9 1,000,000, 10 500,000 and an extra ball
	// (once per game; after that it is the forcefields).
	void hero_skill_shot()
	{
		if (!rs.SkillShotLive)
			return;
		rs.SkillShotLive = false;
		auto ball = ft2_control::CollidingBall;
		if (pb::DevDebug)
			printf("FT2DBG skill shot trigger speed %.1f\n", ball ? ball->Speed : -1.0f);
		if (!ball || ball->Speed >= 100.0f)
			return;
		auto roll = rand() % 10 + 1;
		if (pb::DevDebug && getenv("FT2_SKILLROLL"))
			roll = atoi(getenv("FT2_SKILLROLL"));
		std::string text;
		if (roll <= 5)
		{
			add_points(500000);
			hb.Jackpot = std::min<long long>(5000000, hb.Jackpot + 500000);
			text = hero_text(3051, "500,000! JACKPOT INCREASE!");
		}
		else if (roll >= 8 && roll <= 9)
		{
			add_points(1000000);
			text = hero_text(3053, "1,000,000!");
		}
		else if (roll == 10 && !rs.SkillShotExtraBall)
		{
			add_points(500000);
			rs.SkillShotExtraBall = true;
			table()->ExtraBalls++;
			text = hero_text(3054, "500,000! EXTRA BALL!");
		}
		else
		{
			add_points(500000);
			set_save_sink(true); // forcefields: open until the saver catches a ball
			rs.BallSaveUsed = false;
			text = hero_text(3052, "500,000! FORCEFIELDS!");
		}
		show("SKILL SHOT  " + upper_copy(text), 3.0f);
		light_show(2.0f);
	}

	void set_hit(TPinballComponent* caller)
	{
		build_sets();
		auto found = rs.SetOf.find(caller);
		if (found == rs.SetOf.end())
			return;
		auto index = found->second;
		auto& set = rs.Sets[index];
		bool complete;
		if (set.DropBank)
		{
			complete = true;
			for (auto part : set.Parts)
				if (part->ActiveFlag)
					complete = false;
		}
		else
		{
			if (std::find(set.Hit.begin(), set.Hit.end(), caller) == set.Hit.end())
				set.Hit.push_back(caller);
			complete = set.Hit.size() == set.Parts.size();
		}
		if (!complete)
			return;

		set.Hit.clear();
		auto award = static_cast<int>(set.Parts.size()) * 10000 * rs.Multiplier;
		if (rs.Multiplier < 5)
			rs.Multiplier++;
		add_points(award);
		show((set.DropBank ? "TARGETS COMPLETE  " : "SET COMPLETE  ") + with_commas(award) +
		     "   BONUS x" + std::to_string(rs.Multiplier), 2.5f);
		light_show(1.5f);
		if (!rs.ModePockets.empty())
		{
			open_pockets(20.0f);
			show("POCKETS LIT FOR 20 SECONDS", 2.5f);
		}
		if (!is_hero()) // Captain Hero's henchmen only come out in Mystman's battle
			bring_out_henchman();
		if (set.DropBank)
			timer::set(1.2f, reinterpret_cast<void*>(static_cast<intptr_t>(index)), reset_bank);
		else
			for (auto part : set.Parts)
				if (dynamic_cast<TRollover*>(part))
					part->Message(MessageCode::Reset, 0.0); // lane lights off for the next round
	}

	// Called when a ball is lost for good: end-of-ball bonus, then the per-ball state starts over
	void end_of_ball()
	{
		if (ft2_alien::Active())
			ft2_alien::EndOfBall(); // Alien Daze counts its own bonus
		else if (enhanced() && rs.BonusHits > 0 && !table()->TiltLockFlag)
		{
			auto bonus = rs.BonusHits * 1000 * rs.Multiplier;
			table()->AddScore(bonus);
			show("BONUS " + with_commas(rs.BonusHits * 1000) + " x" + std::to_string(rs.Multiplier) + " = " +
			     with_commas(bonus), 3.0f);
		}
		rs.BonusHits = 0;
		rs.Multiplier = 1;
		rs.BallSaveUsed = false;
		rs.LaunchTick = -1000000;
		rs.PopValue = 1000;
		rs.PopHits = 0;
		if (is_hero() && !(pb::DevDebug && getenv("FT2_KEEPBATTLE"))) // development: keep the battle over drains
			lose_battle();
	}

	void drain_collision()
	{
		auto t = table();
		if (t->MultiballCount > 0 || t->TiltLockFlag)
			return; // other balls still in play
		build_sets();
		if (options::Options.Practice)
		{
			ft2_control::PracticeGame = true;
			end_of_ball();
			show("PRACTICE  BALL BACK", 2.5f);
			return; // same player, same ball number: the drain timer serves a new ball
		}
		auto saveMs = options::Options.BallSave.V * 1000;
		if (saveMs > 0 && !rs.BallSaveUsed && pb::time_ticks - rs.LaunchTick < saveMs)
		{
			rs.BallSaveUsed = true;
			show("BALL SAVED", 2.5f);
			return; // same ball again: the drain timer serves it
		}
		end_of_ball();
		if (t->ExtraBalls)
		{
			t->ExtraBalls--;
			show("SHOOT AGAIN", 2.5f);
			return;
		}
		t->ChangeBallCount(t->BallCount - 1);
		if (t->CurrentPlayer + 1 != t->PlayerCount || t->BallCount)
			t->Message(MessageCode::SwitchToNextPlayer, 0.0);
	}

	void drain_timer()
	{
		auto t = table();
		if (t->CurrentPlayer + 1 == t->PlayerCount && t->BallCount <= 0)
		{
			t->Message(MessageCode::GameOver, 0.0);
			if (pb::InfoTextBox)
				pb::InfoTextBox->Display(pb::get_rc_string(Msg::STRING135), -1.0);
			pb::chk_highscore(); // asks for a name when the score makes this table's top 5
		}
		else if (t->Plunger)
		{
			t->Plunger->Message(MessageCode::PlungerStartFeedTimer, 0.0);
			rs.AwaitingLaunch = true;
		}
	}
}

TBall* ft2_control::CollidingBall = nullptr;
bool ft2_control::PracticeGame = false;

// Captain Hero's shooter ramp (the downtown ramp) is also a ramp you can shoot from the playfield. Its exit diverter
// ("plunger_diverter_return") starts down so the launch can pass; left down, a ball shot up the ramp from the
// playfield rode it all the way back down into the plunger lane. It now comes back up 3 seconds after each launch,
// turning such balls away, and drops again for the next launch.
static int ReturnGateTimer = 0;

static void set_return_gates(bool up)
{
	if (!pb::MainTable)
		return;
	for (auto component : pb::MainTable->ComponentList)
		if (component->GroupName && strstr(component->GroupName, "_return") && dynamic_cast<TGate*>(component))
			component->Message(up ? MessageCode::TGateEnable : MessageCode::TGateDisable, 0.0);
}

static void raise_return_gates(int, void*)
{
	ReturnGateTimer = 0;
	set_return_gates(true);
}

static bool ball_at_plunger()
{
	auto t = pb::MainTable;
	for (auto ball : t->BallList)
		if (ball->ActiveFlag)
		{
			auto dx = ball->Position.X - t->PlungerPosition.X, dy = ball->Position.Y - t->PlungerPosition.Y;
			if (dx * dx + dy * dy < ball->Radius * ball->Radius * 36.0f)
				return true;
		}
	return false;
}

void ft2_control::handler(MessageCode code, TPinballComponent* caller)
{
	auto t = table();
	if (!t || !caller)
		return;

	if (dynamic_cast<TDrain*>(caller))
	{
		if (code == MessageCode::ControlCollision)
			drain_collision();
		else if (code == MessageCode::ControlTimerExpired)
			drain_timer();
		return;
	}

	if ((code == MessageCode::PlungerInputPressed || code == MessageCode::PlungerInputReleased) &&
		dynamic_cast<TPlunger*>(caller))
	{
		if (code == MessageCode::PlungerInputPressed)
		{
			void ft2_control_note_plunger_rest();
			ft2_control_note_plunger_rest();
		}
		for (auto component : t->ComponentList)
			if (auto animation = dynamic_cast<TFT2Animation*>(component))
				if (animation->PlungerLinked)
				{
					if (code == MessageCode::PlungerInputPressed)
						animation->Start();
					else
						animation->Stop();
				}
	}
	if (code == MessageCode::PlungerInputReleased && dynamic_cast<TPlunger*>(caller))
	{
		play_component_sound(caller);
		build_sets();
		if (ball_at_plunger())
		{
			if (ft2_alien::Active())
				ft2_alien::Launch();
			set_return_gates(false);
			if (ReturnGateTimer)
				timer::kill(ReturnGateTimer);
			ReturnGateTimer = timer::set(3.0f, nullptr, raise_return_gates);
		}
		rs.LaunchTick = pb::time_ticks; // ball save counts from the launch
		rs.SkillShotLive = true; // the first pass over Captain Hero's skill shot trigger after a launch decides it
		rs.AwaitingLaunch = false;
		if (rs.SaveSink && !rs.BallSaveUsed && options::Options.BallSave.V > 0)
		{
			set_save_sink(true);
			if (rs.SaveSinkTimer)
				timer::kill(rs.SaveSinkTimer);
			rs.SaveSinkTimer = timer::set(static_cast<float>(options::Options.BallSave.V), nullptr, close_save_sink);
		}
		return;
	}

	// Alien Daze: its own rules (ft2_alien.cpp) take every part they know
	if (ft2_alien::Active())
	{
		if (code == MessageCode::ControlCollision && !t->TiltLockFlag && ft2_alien::Collision(caller))
		{
			rs.BonusHits++;
			return;
		}
		if (code != MessageCode::ControlCollision && ft2_alien::Message(code, caller))
			return;
	}

	if (code == MessageCode::TFlipperExtend && is_hero() && hb.Choosing)
	{
		if (named(caller, {"flpr01"})) // left flipper: the next villain
		{
			hb.ChoiceIndex = (hb.ChoiceIndex + 1) % hb.Choices.size();
			show_choice();
		}
		else if (named(caller, {"flpr02"})) // right flipper: fight this one
			accept_battle(0, nullptr);
		return;
	}

	if (code != MessageCode::ControlCollision)
		return;

	// Captain Hero's scoop: battles start (and Mad Scientist's ends) here; while choosing it keeps the ball
	if (is_hero() && rs.Scoop && caller == rs.Scoop && !t->TiltLockFlag)
	{
		play_component_sound(caller);
		if (hb.Battle == 2)
		{
			win_battle();
			caller->Message(MessageCode::TSinkResetTimer, 2.0f);
			return;
		}
		if (!hb.Battle && !hb.Choosing && t->MultiballCount < 2)
		{
			add_points(points_for(caller));
			offer_battle();
			if (hb.Choosing)
				return; // the ball stays in the scoop until a villain is chosen
			return;
		}
	}

	if (!t->TiltLockFlag)
	{
		play_component_sound(caller);
		auto bumper = dynamic_cast<TBumper*>(caller);
		auto henchman = bumper && bumper->HiddenUntilMode;
		auto points = henchman ? 0 : points_for(caller); // a henchman's hits are scored by henchman_hit
		if (auto pops = pop_rule(caller))
			points = pop_points(*pops);
		if (is_hero())
		{
			auto battlePoints = hero_battle_hit(caller);
			if (battlePoints >= 0)
				points = battlePoints;
			if (named(caller, {"ss_trigger"}))
				hero_skill_shot();
			else if (rs.SkillShotLive && !named(caller, {"downtown_ramp"}))
				rs.SkillShotLive = false; // the ball went somewhere else first
		}
		if (points)
		{
			add_points(points);
			rs.BonusHits++;
		}
		if (henchman)
			henchman_hit(bumper);
		if (enhanced())
			set_hit(caller);
	}

	if (rs.SaveSink && caller == rs.SaveSink)
	{
		rs.BallSaveUsed = true;
		set_save_sink(false); // shut again once the ball is thrown back out (it stays inside until then)
		show("BALL SAVED", 2.5f);
	}

	// Captured balls: send them back out after a short pause
	if (dynamic_cast<TKickout*>(caller))
		caller->Message(MessageCode::TKickoutRestartTimer, 1.0f);
	else if (dynamic_cast<TSink*>(caller))
		caller->Message(MessageCode::TSinkResetTimer, 1.0f);
}

std::vector<std::string> ft2_control::StatusLines()
{
	std::vector<std::string> lines;
	auto t = table();
	if (!t || rs.Table != t || pb::game_mode != GameModes::InGame)
		return lines;
	if (options::Options.Practice)
		lines.emplace_back("PRACTICE  (balls come back)");
	if (is_hero() && rs.AwaitingLaunch)
		lines.emplace_back("Skill shot: short pull");
	if (is_hero())
	{
		if (hb.Choosing)
			lines.emplace_back("Pick a villain: left flipper next, right flipper fight");
		else if (hb.Battle)
		{
			auto index = hb.Battle - 1;
			lines.push_back("Battle " + villain(index) + ": " + VillainGoals[index] + " (" +
			                std::to_string(VillainGoal[index] - hb.Progress) + " to go)");
		}
		else
			lines.emplace_back("Shoot the scoop to battle a villain");
		lines.push_back("Jackpot " + with_commas(hb.Jackpot));
		if (hb.Captured)
			lines.push_back("Villains captured: " + std::to_string(hb.Captured));
		if (rs.SaveSink && rs.SaveSink->ActiveFlag)
			lines.emplace_back("Forcefields on (ball saver open)");
	}
	if (ft2_alien::Active())
	{
		for (auto& line : ft2_alien::Status())
			lines.push_back(line);
		return lines;
	}
	if (enhanced() && rs.Multiplier > 1)
		lines.push_back("Bonus x" + std::to_string(rs.Multiplier));
	if (rs.PocketTimer)
		lines.emplace_back("Pockets lit");
	if (rs.PopValue > 1000)
		lines.push_back("Pop bumpers " + with_commas(rs.PopValue));
	int out = 0, mostHits = 0;
	for (auto henchman : rs.Henchmen)
		if (henchman->Shown)
		{
			out++;
			if (rs.HenchmanHits.count(henchman))
				mostHits = std::max(mostHits, rs.HenchmanHits.at(henchman));
		}
	if (out)
		lines.push_back((out == 1 ? std::string("Enemy out") : std::to_string(out) + " enemies out") + ": " +
		                std::to_string(mostHits) + " of " + std::to_string(HenchmanHitsNeeded) + " hits");
	return lines;
}

// Blockers the original rules keep raised because they close off part of the table. Skulduggery's a_blocf closes the
// bottom of the upper playfield except for the gap onto its exit ramp; lowered, balls rolled off the table there.
static void raise_blockers()
{
	static const struct { const char* Table; const char* Blocker; } raised[] =
	{
		{"PIRATES.DAT", "a_blocf"},
	};
	if (!pb::MainTable)
		return;
	std::string key = pb::DatFileName;
	auto slash = key.find_last_of("/\\");
	if (slash != std::string::npos)
		key = key.substr(slash + 1);
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	for (auto& entry : raised)
		if (key == entry.Table)
			for (auto component : pb::MainTable->ComponentList)
				if (component->GroupName && strcmp(component->GroupName, entry.Blocker) == 0 &&
					dynamic_cast<TBlocker*>(component))
					component->Message(MessageCode::TBlockerEnable, 0.0);
}

void ft2_control::table_handler(MessageCode code)
{
	if (!table())
		return;
	build_sets();
	switch (code)
	{
	case MessageCode::Reset: // new game
		raise_blockers();
		if (ft2_alien::Active())
			ft2_alien::NewGame();
		stop_attract();
		if (rs.PocketTimer)
		{
			timer::kill(rs.PocketTimer);
			rs.PocketTimer = 0;
		}
		set_pockets(false);
		rs.HenchmanHits.clear(); // the henchmen leave the table on Reset themselves
		if (pb::DevDebug && getenv("FT2_HENCHMEN")) // development: start with every henchman out
			while (bring_out_henchman())
			{
			}
		for (auto& set : rs.Sets)
			set.Hit.clear();
		for (auto& given : rs.ExtraBallGiven)
			given = false;
		rs.BonusHits = 0;
		rs.Multiplier = 1;
		rs.BallSaveUsed = false;
		rs.LaunchTick = -1000000;
		rs.PopValue = 1000;
		rs.PopHits = 0;
		rs.SkillShotLive = false;
		rs.SkillShotExtraBall = false;
		rs.AwaitingLaunch = true;
		ft2_control::PracticeGame = options::Options.Practice;
		set_save_sink(false);
		lose_battle();
		hb = HeroBattles{};
		if (pb::DevDebug && getenv("FT2_BATTLE") && is_hero()) // development: start a battle at once
		{
			start_battle(std::max(0, std::min(VillainCount - 1, atoi(getenv("FT2_BATTLE")) - 1)));
			if (getenv("FT2_BATTLEPROGRESS"))
				hb.Progress = atoi(getenv("FT2_BATTLEPROGRESS"));
		}
		if (pb::DevDebug && getenv("FT2_OFFER") && is_hero()) // development: the villain choice at once
			offer_battle();
		break;
	case MessageCode::GameOver:
		if (enhanced() && !rs.AttractTimer)
			rs.AttractTimer = timer::set(3.5f, nullptr, attract_tick); // lights play while waiting for a new game
		break;
	case MessageCode::SwitchToNextPlayer:
		raise_blockers(); // blockers drop when the player changes
		if (ft2_alien::Active())
			ft2_alien::PlayerChanged();
		for (auto& set : rs.Sets)
			set.Hit.clear();
		break;
	default:
		break;
	}
}

void ft2_control::check_escaped_balls()
{
	auto t = pb::MainTable;
	if (!t)
		return;
	if (ft2_alien::Active())
		ft2_alien::Frame();
	static TTableLayer* layer = nullptr;
	static TPinballTable* layerTable = nullptr;
	if (layerTable != t)
	{
		layerTable = t;
		layer = nullptr;
		for (auto component : t->ComponentList)
			if ((layer = dynamic_cast<TTableLayer*>(component)) != nullptr)
				break;
	}
	if (!layer)
		return;
	for (auto ball : t->BallList)
	{
		if (!ball->ActiveFlag || ball->CollisionComp)
			continue;
		auto margin = ball->Radius * 4.0f;
		auto& p = ball->Position;
		bool outside = p.X < layer->XMin - margin || p.X > layer->XMax + margin ||
			p.Y < layer->YMin - margin || p.Y > layer->YMax + margin || p.X != p.X || p.Y != p.Y;
		if (!outside)
			continue;
		ftlog::Line("Ball left the table at " + std::to_string(p.X) + ", " + std::to_string(p.Y) + " (layer mask " +
			std::to_string(ball->CollisionMask) + "): given back at the plunger");
		ball->Disable();
		int active = 0;
		for (auto other : t->BallList)
			if (other->ActiveFlag)
				active++;
		if (active == 0 && t->Plunger)
		{
			show("BALL RETURNED", 2.0f);
			t->Plunger->Message(MessageCode::PlungerStartFeedTimer, 0.0);
		}
		else if (t->MultiballCount > 0)
			t->MultiballCount--;
	}
}

bool ft2_control::AwaitingLaunch()
{
	return rs.AwaitingLaunch;
}

static bool HavePlungerRest = false;
static float PlungerRestX = 0, PlungerRestY = 0;

bool ft2_control::PlungerRest(float& x, float& y)
{
	x = PlungerRestX;
	y = PlungerRestY;
	return HavePlungerRest;
}

void ft2_control_note_plunger_rest()
{
	auto t = pb::MainTable;
	if (!t || !t->Plunger)
		return;
	for (auto ball : t->BallList)
		if (ball->ActiveFlag && !ball->CollisionComp && ball->Speed < ball->Radius)
		{
			auto dx = ball->Position.X - t->PlungerPosition.X, dy = ball->Position.Y - t->PlungerPosition.Y;
			if (dx * dx + dy * dy < ball->Radius * ball->Radius * 225.0f)
			{
				PlungerRestX = ball->Position.X;
				PlungerRestY = ball->Position.Y;
				HavePlungerRest = true;
				return;
			}
		}
}
