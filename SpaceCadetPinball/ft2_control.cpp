#include "pch.h"
#include "ft2_control.h"
#include "ft2_scores.h"
#include "ft2_sounds.h"
#include "ft2_animation.h"
#include "ft2_alien.h"
#include "ft2_mad.h"

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
#include "TFlipper.h"
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
		if (pb::DevDebug)
			printf("FT2DBG show: %s\n", text.c_str());
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
		if (ft2_mad::Active())
			open = true; // Mad Scientist's kickouts are always in play; its rules decide what they do
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

	const PopRule* table_pop_rule()
	{
		auto key = table_key();
		for (auto& rule : PopRules)
			if (key == rule.Table)
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

	// ---- Captain Hero (hero.exe, source hero\src\control.cpp; notes in ft2/hero_rules.md) ----
	// Battles (current battle 0x43d088, start 0x40787c, end 0x407e08). Shoot the scoop (s_sink01, 0x406580) when no
	// battle is on: it holds the ball and offers the villains not yet captured (left flipper: next, right flipper:
	// accept, 20 s: taken as it is). Battles 1-5 pay 100,000 to start and last 60 seconds; the scoop during one
	// restarts the 60 seconds ("Extend Battle"). Won by its own shots: 1 Quake the nine drop targets, 2 Mad Scientist
	// the scoop again, 3 Mystman a henchman out after 4 s, hit 5 times, 4 Slime Man 8 pop bumper hits, 5 Mr. Pling 8
	// hits on a_rebo04. A capture pays the Jackpot and adds 250,000 x the multiplier to the Super Jackpot. Evil Twin
	// (6) is only offered once the other five are captured (0x406364) and has no time limit: knock out the five
	// pieces of Krypton-@ (dynamite targets a_drop18..21 and s_kout01, 200,000 each, 0x406c14); s_kout01 lights the
	// Super Jackpot (0x406294), and three downtown ramp shots collect it with a four-ball multiball (0x408638). The
	// battle ends with the ball; afterwards every villain comes back.
	const int VillainCount = 6;
	const int EvilTwin = 5; // index
	const char* const VillainNames[VillainCount] = {"Quake", "Mad Scientist", "Mystman", "Slime Man", "Mr. Pling",
	                                                 "Evil Twin"};
	const char* const VillainGoals[VillainCount] = {"knock down the 9 drop targets", "shoot the scoop again",
	                                                 "hit the henchman 5 times", "hit the pop bumpers 8 times",
	                                                 "hit Mr. Pling's target 8 times",
	                                                 "blow up Krypton-@"};
	const int VillainGoal[VillainCount] = {9, 1, 5, 8, 8, 5};
	const int CaptureTexts[VillainCount] = {3069, 3071, 3073, 3075, 3077, 0};
	const float BattleSeconds = 60.0f; // 0x42700000
	const long long JackpotStart = 500000, JackpotMax = 5000000;           // table +0x141
	const long long SuperJackpotStart = 1000000, SuperJackpotMax = 20000000; // table +0x149

	// Ten buildings (btrig01..10, damage 0x449928: 0 standing, 1 damaged, 2 wrecked)
	const int BuildingCount = 10;
	// Mystery award at s_sink03 (0x404768, weights 0x4414b8 in percent)
	const int MysteryWeight[6] = {50, 25, 15, 5, 3, 2};
	const int MysteryText[6] = {3025, 3026, 3027, 3028, 3029, 3030};
	const char* const MysteryFallback[6] = {"50,000", "100,000", "1 Million", "All Buildings Repaired!",
	                                        "Max Bumpers!", "EXTRA BALL!!!"};

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
		long long Jackpot = JackpotStart;
		long long SuperJackpot = SuperJackpotStart;
		int SuperMultiplier = 1; // 0x44160c, raised at s_sink03 up to 5
		long long Captured = 0;
		int BattleTimer = 0;
		int BattleEnds = 0; // pb::time_ticks
		// mystery (light_74 flashes for 5 s after a battle starts, 0x40496c)
		bool MysteryLit = false;
		int MysteryTimer = 0;
		// buildings
		int Damage[BuildingCount]{};
		int ThrownBy = 0; // the original's ball +0x140: the part that threw the ball (0 none, 6 = wrecks a building)
		int ThrownTick = 0;
		// Be A Hero (s_kout02, 0x4045a4)
		int HeroValue = 30000;
		int Deed = 0;
		// extra ball (s_sink10 lights it, s_sink14 collects it; once per game: 0x43d080)
		bool ExtraBallLit = false, ExtraBallGiven = false;
		// bumper boost (s_sink04 + s_sink05, 0x404344 / 0x404470)
		bool Boost04 = false, Boost05 = false;
		// downtown ramp combo (0x405bf8): 30,000, 30,000 more each shot while light_49 is on (10 s)
		long long RampValue = 30000;
		int RampComboUntil = 0;
		bool RampPassAfterLaunch = false; // the launch itself goes up the downtown ramp: not a shot
		// Evil Twin
		bool Krypton[5]{};
		int SuperLit = 0; // 0x441a44: 1 lit, 2 collected
		int SuperShots = 0;
		// this ball's bonus (0x441224 battles, 0x441228 captures)
		int BallBattles = 0, BallCaptures = 0;
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
			if (out.size() >= 2 && out.back() == ' ' && out[out.size() - 2] == ' ')
				out.pop_back();
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

	TPinballComponent* hero_part(const char* name)
	{
		static std::map<std::string, TPinballComponent*> cache;
		static TPinballTable* cacheTable = nullptr;
		if (cacheTable != table())
		{
			cache.clear();
			cacheTable = table();
		}
		auto found = cache.find(name);
		if (found != cache.end())
			return found->second;
		TPinballComponent* part = nullptr;
		for (auto component : table()->ComponentList)
			if (component->GroupName && strcmp(component->GroupName, name) == 0)
			{
				part = component;
				break;
			}
		cache[name] = part;
		return part;
	}

	void hero_light(const char* name, bool on)
	{
		if (auto light = hero_part(name))
			light->Message(on ? MessageCode::TLightResetAndTurnOn : MessageCode::TLightResetAndTurnOff, 0.0f);
	}

	void hero_flash(const char* name)
	{
		if (auto light = hero_part(name))
			light->Message(MessageCode::TLightFlasherStart, 0.0f);
	}

	void hero_kill(int& timerId)
	{
		if (timerId)
			timer::kill(timerId);
		timerId = 0;
	}

	bool battle_on()
	{
		return hb.Battle >= 1 && hb.Battle <= 6;
	}

	// The jackpots grow with play (0x40764c): targets 5,000 (0x4076d8), holes 10,000 (0x407738); during a battle
	// the Super Jackpot grows too, by 30,000 / 40,000 times its multiplier
	void grow_jackpots(bool hole)
	{
		hb.Jackpot = std::min(JackpotMax, hb.Jackpot + (hole ? 10000 : 5000));
		if (battle_on())
			hb.SuperJackpot = std::min(SuperJackpotMax,
			                           hb.SuperJackpot + (hole ? 40000LL : 30000LL) * hb.SuperMultiplier);
	}

	void hide_henchmen()
	{
		hero_kill(hb.HenchmanTimer);
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

	void mystery_off(int, void*)
	{
		hb.MysteryTimer = 0;
		hb.MysteryLit = false;
		hero_light("light_74", false);
	}

	void battle_timeout(int, void*);

	// Mr. Pling's three walls (a_rebo03/04/05) are only there during his battle (0x405944 / 0x405ac4 / 0x407e08 write
	// their active flag). Left on, they knocked the ball from s_sink04's exit straight into s_sink05 over and over.
	void set_pling(bool on)
	{
		for (auto name : {"a_rebo03", "a_rebo04", "a_rebo05"})
			if (auto wall = hero_part(name))
				wall->ActiveFlag = on;
		hero_light("light_pl", on);
	}

	void start_battle_timer()
	{
		hero_kill(hb.BattleTimer);
		hb.BattleTimer = timer::set(BattleSeconds, nullptr, battle_timeout);
		hb.BattleEnds = pb::time_ticks + static_cast<int>(BattleSeconds * 1000);
	}

	void krypton_start()
	{
		for (auto& piece : hb.Krypton)
			piece = true;
		hb.SuperLit = 0;
		hb.SuperShots = 0;
		for (auto name : {"light_73", "light_71", "light_93", "light_95", "light_94"})
			hero_light(name, true);
		hero_flash("light_53");
	}

	void start_battle(int index)
	{
		if (pb::DevDebug)
			printf("FT2DBG battle start %d %s\n", index + 1, VillainNames[index]);
		hb.Battle = index + 1;
		hb.Progress = 0;
		for (auto& down : hb.QuakeDown)
			down = false;
		hb.BallBattles++;
		add_points(100000);
		// the mystery light flashes for 5 seconds (0x40496c)
		hb.MysteryLit = true;
		hero_flash("light_74");
		hero_kill(hb.MysteryTimer);
		hb.MysteryTimer = timer::set(5.0f, nullptr, mystery_off);
		if (index == EvilTwin)
		{
			krypton_start();
			show(upper_copy(hero_text(3079, "Blow Up Krypton-@")), 3.5f);
		}
		else
		{
			start_battle_timer();
			show("BATTLE " + upper_copy(villain(index)) + "  " + upper_copy(VillainGoals[index]), 3.5f);
		}
		light_show(1.5f);
		if (hb.Battle == 3)
			hb.HenchmanTimer = timer::set(4.0f, nullptr, henchman_arrives);
		if (hb.Battle == 5)
			set_pling(true); // APPROXIMATION: the original first sends a second ball up s_ramp02 into Mr. Pling
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
		hero_kill(hb.ChooseTimer);
		if (pb::InfoTextBox)
			pb::InfoTextBox->Clear();
		start_battle(hb.Choices[hb.ChoiceIndex]);
		if (rs.Scoop && !(pb::DevDebug && getenv("FT2_OFFER")))
			rs.Scoop->Message(MessageCode::TSinkResetTimer, 2.0f); // the ball comes back out
	}

	// 0x406364: the villains not yet captured; Evil Twin only once all five are
	void offer_battle()
	{
		hb.Choices.clear();
		for (int i = 0; i < EvilTwin; i++)
			if (!hb.Defeated[i])
				hb.Choices.push_back(i);
		if (hb.Choices.empty())
			hb.Choices.push_back(EvilTwin);
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

	void end_battle_common()
	{
		if (hb.Battle == 5)
			set_pling(false);
		hb.Battle = 0;
		hb.Progress = 0;
		hero_kill(hb.BattleTimer);
		hb.BattleEnds = 0;
		hide_henchmen();
	}

	void win_battle()
	{
		auto index = hb.Battle - 1;
		if (index < 0 || index == EvilTwin)
			return;
		if (pb::DevDebug)
			printf("FT2DBG battle won %d jackpot %lld\n", hb.Battle, hb.Jackpot);
		hb.Defeated[index] = true;
		hb.Captured++;
		hb.BallCaptures++;
		end_battle_common();
		// a capture adds 250,000 x the multiplier to the Super Jackpot (0x4077c8), then pays the Jackpot
		hb.SuperJackpot = std::min(SuperJackpotMax, hb.SuperJackpot + 250000LL * hb.SuperMultiplier);
		auto award = hb.Jackpot;
		add_points(static_cast<int>(std::min<long long>(award, 2000000000)));
		auto fallback = std::string(VillainNames[index]) + " Captured, Jackpot Awarded!!!";
		auto text = CaptureTexts[index] ? hero_text(CaptureTexts[index], fallback) : fallback;
		show(upper_copy(text) + "  " + with_commas(award), 4.0f);
		light_show(3.0f);
		hb.Jackpot = JackpotStart;
	}

	void battle_progress(int step = 1)
	{
		if (!hb.Battle || hb.Battle - 1 == EvilTwin)
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

	// Evil Twin's battle ends with the ball (0x407e08 case 6); either way every villain comes back (0x406b08)
	void end_evil_twin()
	{
		auto won = hb.SuperLit == 2;
		end_battle_common();
		for (auto name : {"light_73", "light_71", "light_93", "light_95", "light_94", "light_53", "light_49"})
			hero_light(name, false);
		for (auto& piece : hb.Krypton)
			piece = false;
		hb.SuperLit = 0;
		hb.SuperShots = 0;
		for (auto& defeated : hb.Defeated)
			defeated = false;
		show(won ? "EVIL TWIN DEFEATED  THE VILLAINS ARE BACK" : "EVIL TWIN GOT AWAY", 3.0f);
	}

	void lose_battle()
	{
		if (hb.Choosing)
		{
			hb.Choosing = false;
			hero_kill(hb.ChooseTimer);
		}
		if (!hb.Battle)
			return;
		if (pb::DevDebug)
			printf("FT2DBG battle lost %d at %.1f s (started %.1f s)\n", hb.Battle, pb::time_ticks / 1000.0f,
			       (hb.BattleEnds - BattleSeconds * 1000) / 1000.0f);
		if (hb.Battle - 1 == EvilTwin)
		{
			end_evil_twin();
			return;
		}
		show(upper_copy(villain(hb.Battle - 1)) + " GOT AWAY", 2.5f);
		end_battle_common();
	}

	void battle_timeout(int, void*)
	{
		hb.BattleTimer = 0;
		if (battle_on() && hb.Battle - 1 != EvilTwin)
			lose_battle();
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

	// ---- Buildings (0x40888c repair, 0x40895c / 0x408a1c damage, 0x408ac8 / 0x408b28 building triggers) ----
	bool any_damage()
	{
		for (auto damage : hb.Damage)
			if (damage)
				return true;
		return false;
	}

	// buildings not wrecked (0x408b08), for the end-of-ball bonus
	int standing_buildings()
	{
		int count = 0;
		for (auto damage : hb.Damage)
			if (damage != 2)
				count++;
		return count;
	}

	// One damaged building, picked at random, is fully repaired
	void repair_one()
	{
		std::vector<int> damaged;
		for (int i = 0; i < BuildingCount; i++)
			if (hb.Damage[i])
				damaged.push_back(i);
		if (damaged.empty())
			return;
		hb.Damage[damaged[rand() % damaged.size()]] = 0;
		if (!any_damage())
			show(upper_copy(hero_text(3028, "All Buildings Repaired!")), 2.0f);
	}

	void repair_all()
	{
		for (auto& damage : hb.Damage)
			damage = 0;
	}

	// A ball thrown by one of the parts below that then crosses a building's trigger damages that building; one
	// thrown by a magnet pocket (6) wrecks it
	void ball_thrown(int kind)
	{
		hb.ThrownBy = kind;
		hb.ThrownTick = pb::time_ticks;
	}

	void building_hit(int index)
	{
		auto kind = hb.ThrownBy;
		hb.ThrownBy = 0;
		// APPROXIMATION: the 4 s limit is not from the original, which clears the mark in engine code not read yet
		if (!kind || pb::time_ticks - hb.ThrownTick > 4000 || index < 0 || index >= BuildingCount)
			return;
		auto& damage = hb.Damage[index];
		if (damage >= 2)
			return;
		damage = kind == 6 ? 2 : damage + 1;
		if (pb::DevDebug)
			printf("FT2DBG building %d damage %d\n", index + 1, damage);
		show(upper_copy(hero_text(3001, "Hit Bumpers to Repair Buildings")), 2.5f);
	}

	// ---- Mystery award (0x404768) and the s_sink03 hole (0x404840) ----
	void mystery_award()
	{
		mystery_off(0, nullptr);
		hero_kill(hb.MysteryTimer);
		auto roll = rand() % 100;
		int pick = 0;
		for (; pick < 5; pick++)
		{
			if (roll < MysteryWeight[pick])
				break;
			roll -= MysteryWeight[pick];
		}
		if (pick == 5 && (hb.ExtraBallGiven || table()->ExtraBalls > 1))
			pick = 0; // the extra ball only when none is waiting
		show(upper_copy(hero_text(3032, "Mystery Award!")) + "  " + upper_copy(hero_text(MysteryText[pick],
		     MysteryFallback[pick])), 3.0f);
		switch (pick)
		{
		case 0: add_points(50000); break;
		case 1: add_points(100000); break;
		case 2: add_points(1000000); break;
		case 3: repair_all(); break;
		case 4: rs.PopValue = 5000; rs.PopHits = 0; break; // max bumpers (0x4022e0)
		default:
			table()->ExtraBalls++;
			hb.ExtraBallGiven = true;
			break;
		}
	}

	// ---- Evil Twin: Krypton-@ (0x406954, 0x406c14) and the Super Jackpot (0x408638) ----
	bool krypton_piece()
	{
		for (int i = 0; i < 5; i++)
			if (hb.Krypton[i])
			{
				hb.Krypton[i] = false;
				const char* lights[5] = {"light_73", "light_71", "light_93", "light_95", "light_94"};
				hero_light(lights[i], false);
				add_points(200000);
				int left = 0;
				for (auto piece : hb.Krypton)
					left += piece;
				show("KRYPTON-@  200,000" + (left ? "  " + std::to_string(left) + " LEFT" : std::string()), 1.5f);
				return true;
			}
		return false;
	}

	void add_ball_at_sink(const char* name)
	{
		auto sink = dynamic_cast<TSink*>(hero_part(name));
		auto t = table();
		if (!sink)
			return;
		if (auto ball = t->AddBall(sink->BallPosition))
		{
			ball->throw_ball(&sink->BallThrowDirection, sink->ThrowAngleMult, sink->ThrowSpeedMult1,
			                 sink->ThrowSpeedMult2);
			t->MultiballCount++;
		}
	}

	void super_jackpot_shot()
	{
		if (hb.SuperLit != 1)
			return;
		if (++hb.SuperShots < 3)
		{
			show("SUPER JACKPOT  " + std::to_string(3 - hb.SuperShots) + " MORE", 1.5f);
			return;
		}
		hb.SuperLit = 2;
		hero_light("light_49", false);
		auto award = hb.SuperJackpot;
		add_points(static_cast<int>(std::min<long long>(award, 2000000000)));
		show(upper_copy(hero_text(3065, "SUPER JACKPOT!!!")) + "  " + with_commas(award), 4.0f);
		light_show(4.0f);
		hb.SuperJackpot = SuperJackpotStart;
		// APPROXIMATION: the original (0x408638) gives the s_kout01 award three times (0x4061f0, 75,000 each with
		// its own text, not ported), sends "eject" (67) to s_sink03/05/04/01 and adds 4 to the balls-in-play count
		// (table +0x161). Where the extra balls come from is not read yet; here three balls are added at the holes.
		for (auto name : {"s_sink03", "s_sink05", "s_sink04"})
			add_ball_at_sink(name);
	}

	// The rules for one Captain Hero hit (rule table in ft2/hero_controls.txt): returns the points to score.
	// `points` is the part's own value from the rule table.
	int hero_hit(TPinballComponent* caller, int points)
	{
		auto now = pb::time_ticks;

		// buildings first: the trigger is not a hit of its own
		const char* triggers[BuildingCount] = {"btrig01", "btrig02", "btrig03", "btrig04", "btrig05",
		                                       "btrig06", "btrig07", "btrig08", "btrig09", "btrig10"};
		for (int i = 0; i < BuildingCount; i++)
			if (named(caller, {triggers[i]}))
			{
				building_hit(i);
				return 0;
			}

		// parts that throw the ball where it can land on a building (writes of the ball's +0x140)
		if (named(caller, {"a_spot09", "a_spot10", "a_spot11", "a_spot12", "a_spot13", "a_spot14", "a_spot15",
		                   "a_spot16"}))
			ball_thrown(1);
		else if (caller->GroupName && strncmp(caller->GroupName, "a_drop", 6) == 0 &&
		         atoi(caller->GroupName + 6) >= 29 && atoi(caller->GroupName + 6) <= 46)
			ball_thrown(2);
		else if (named(caller, {"a_rebo03", "a_rebo04", "a_rebo05"}))
			ball_thrown(5);
		else if (named(caller, {"s_kout03", "s_kout04", "s_kout05"}))
			ball_thrown(4);
		else if (named(caller, {"s_kout06", "s_kout07", "s_kout08", "s_kout09", "s_kout10"}))
			ball_thrown(6);
		else
			hb.ThrownBy = 0;

		// pop bumpers repair the city (0x4023c8): on half their hits, and always when their value goes up, one
		// building per 1,000 of their value
		if (named(caller, {"a_bump01", "a_bump02", "a_bump03"}))
		{
			// APPROXIMATION: "half the hits" is a reading of a condition the decompiler left unclear (0x4023c8)
			if (any_damage() && (rand() % 2 == 0 || points >= rs.PopValue))
				for (int i = 0; i < points / 1000; i++)
					repair_one();
		}

		// targets grow the jackpots by 5,000, holes by 10,000
		if (caller->GroupName && (strncmp(caller->GroupName, "a_drop", 6) == 0 ||
		                          strncmp(caller->GroupName, "a_spot", 6) == 0))
			grow_jackpots(false);
		else if (named(caller, {"s_sink03", "s_sink04", "s_sink05", "s_sink08", "s_sink09", "s_sink13", "s_sink14",
		                        "s_sink15", "s_sink20"}))
			grow_jackpots(true);

		// the battles
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
			{
				battle_progress();
				return 50000; // 0x405944
			}
			break;
		case 6:
			if (named(caller, {"a_drop18", "a_drop19", "a_drop20", "a_drop21"}) && hb.SuperLit == 0)
				krypton_piece(); // the dynamite targets (0x402e3c)
			else if (named(caller, {"s_kout01"}) && hb.SuperLit == 0)
			{
				krypton_piece();
				hb.SuperLit = 1; // 0x406294: the Super Jackpot is lit
				hb.SuperShots = 0;
				hero_light("light_53", false);
				hero_flash("light_49");
				show("SUPER JACKPOT LIT  SHOOT THE DOWNTOWN RAMP 3 TIMES", 3.0f);
			}
			break;
		default:
			break;
		}

		// Be A Hero (s_kout02, 0x4045a4): 30,000, 10,000 more each time up to 150,000, and a good deed
		if (named(caller, {"s_kout02"}))
		{
			auto value = hb.HeroValue;
			if (hb.HeroValue < 150000)
				hb.HeroValue += 10000;
			auto deed = hero_text(3007 + hb.Deed, "Be A Hero!");
			hb.Deed = (hb.Deed + 1) % 18;
			show(upper_copy(deed) + "  " + with_commas(value), 3.0f);
			return value;
		}

		// s_sink03 (0x404840): the mystery award while its light flashes, else the Super Jackpot multiplier goes up
		if (named(caller, {"s_sink03"}))
		{
			if (hb.MysteryLit)
				mystery_award();
			else if (hb.SuperMultiplier < 5)
			{
				hb.SuperMultiplier++;
				show(upper_copy(hero_text(3031, "Super Jackpot Multiplier Increased")) + "  X" +
				     std::to_string(hb.SuperMultiplier), 2.0f);
			}
			return 0;
		}

		// s_sink10 (0x404b28) 25,000 and lights the extra ball; s_sink14 (0x404258) 50,000 and collects it
		if (named(caller, {"s_sink10"}))
		{
			if (!hb.ExtraBallGiven && !hb.ExtraBallLit)
			{
				hb.ExtraBallLit = true;
				hero_light("light_59", true);
				show(upper_copy(hero_text(3055, "Extra Ball Available")), 2.5f);
			}
			return 25000;
		}
		if (named(caller, {"s_sink14"}))
		{
			if (hb.ExtraBallLit)
			{
				hb.ExtraBallLit = false;
				hb.ExtraBallGiven = true;
				hero_light("light_59", false);
				table()->ExtraBalls++;
				show(upper_copy(hero_text(3004, "EXTRA BALL!!!")), 3.0f);
				light_show(2.0f);
			}
			return 50000;
		}

		// s_sink04 / s_sink05 (0x404470 / 0x404344): 60,000 each; both lit boosts the pop bumpers (125,000)
		if (named(caller, {"s_sink04", "s_sink05"}))
		{
			(named(caller, {"s_sink04"}) ? hb.Boost04 : hb.Boost05) = true;
			hero_light(named(caller, {"s_sink04"}) ? "light_47" : "light_46", true);
			if (hb.Boost04 && hb.Boost05)
			{
				hb.Boost04 = hb.Boost05 = false;
				hero_light("light_46", false);
				hero_light("light_47", false);
				rs.PopValue = std::min(5000, std::max(1000, rs.PopValue) + 1000); // 0x4022bc
				rs.PopHits = 0;
				show(upper_copy(hero_text(3005, "Bumpers Boosted!")), 2.0f);
				return 125000;
			}
			return 60000;
		}

		// the downtown ramp (0x405bf8)
		if (named(caller, {"downtown_ramp"}))
		{
			if (hb.RampPassAfterLaunch)
			{
				hb.RampPassAfterLaunch = false;
				return 0;
			}
			if (hb.Battle - 1 == EvilTwin && hb.SuperLit == 1)
			{
				super_jackpot_shot();
				return 0;
			}
			hb.RampValue = now < hb.RampComboUntil ? hb.RampValue + 30000 : 30000;
			hb.RampComboUntil = now + 10000;
			hero_flash("light_49");
			if (hb.RampValue > 30000)
				show("RAMP COMBO  " + with_commas(hb.RampValue), 1.5f);
			return static_cast<int>(hb.RampValue);
		}

		return points;
	}

	// End of ball (0x406f58): 50,000 per standing building, 100,000 per battle, 250,000 per capture this ball
	void hero_end_of_ball()
	{
		auto t = table();
		auto buildings = standing_buildings();
		long long bonus = buildings * 50000LL + hb.BallBattles * 100000LL + hb.BallCaptures * 250000LL;
		if (!t->TiltLockFlag && bonus > 0)
		{
			t->AddScore(static_cast<int>(bonus));
			show("BONUS  BATTLES " + std::to_string(hb.BallBattles) + "  CAPTURES " + std::to_string(hb.BallCaptures) +
			     "  BUILDINGS " + std::to_string(buildings) + "  TOTAL " + with_commas(bonus), 3.5f);
		}
		hb.BallBattles = 0;
		hb.BallCaptures = 0;
		hb.SuperMultiplier = 1; // reset with the ball (0x404840, 1017)
		hb.HeroValue = 30000;
		hb.RampValue = 30000;
		hb.RampComboUntil = 0;
		hb.Boost04 = hb.Boost05 = false;
		hero_light("light_46", false);
		hero_light("light_47", false);
		hb.ThrownBy = 0;
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
			hb.Jackpot = std::min(JackpotMax, hb.Jackpot + 500000);
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
		if (ft2_mad::Active())
			ft2_mad::EndOfBall(); // and so does Mad Scientist
		else if (is_hero())
			hero_end_of_ball(); // and Captain Hero
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
		if (ft2_alien::Active() && ft2_alien::SaveBall())
			return; // Alien Daze's shoot again light
		if (ft2_mad::Active() && ft2_mad::SaveBall())
			return; // Mad Scientist's shoot again light or ball recapture

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

// Development (FT2_DEBUG): FT2_HITS="part,part,..." sends one hit to the rules every 0.6 s after New Game, so each
// rule can be checked without aiming a ball. "wait" skips a beat, "flpr01"/"flpr02" press a flipper, "drain" loses
// the ball (the rules' end of ball, then a new ball).
static void scripted_hit(int, void*)
{
	static size_t next = 0;
	static std::vector<std::string> hits;
	if (hits.empty())
	{
		std::string list = getenv("FT2_HITS");
		size_t start = 0;
		while (start <= list.size())
		{
			auto comma = list.find(',', start);
			hits.push_back(list.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
			if (comma == std::string::npos)
				break;
			start = comma + 1;
		}
	}
	if (next >= hits.size() || !pb::MainTable)
		return;
	auto name = hits[next++];
	printf("FT2DBG scripted %s\n", name.c_str());
	if (name == "drain")
		end_of_ball();
	else if (name != "wait")
		for (auto component : pb::MainTable->ComponentList)
			if (component->GroupName && name == component->GroupName)
			{
				auto code = dynamic_cast<TFlipper*>(component) ? MessageCode::TFlipperExtend : MessageCode::ControlCollision;
				ft2_control::handler(code, component);
				break;
			}
	timer::set(0.6f, nullptr, scripted_hit);
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

	// A ramp that ends in a hole ("element_b_ramp" drops the ball through "element_b_ramp_hole", Mad Scientist) has
	// no end wall to report the shot; the ball falling into the hole is the ramp made
	if (code == MessageCode::ControlBallCaptured && dynamic_cast<THole*>(caller) && caller->GroupName)
	{
		std::string name = caller->GroupName;
		if (name.size() > 5 && name.compare(name.size() - 5, 5, "_hole") == 0)
		{
			name.resize(name.size() - 5);
			for (auto component : t->ComponentList)
				if (component->GroupName && name == component->GroupName && dynamic_cast<TRamp*>(component))
				{
					handler(MessageCode::ControlCollision, component);
					break;
				}
		}
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
			if (ft2_mad::Active())
				ft2_mad::Launch();
			if (is_hero())
				hb.RampPassAfterLaunch = true; // the launch goes up the downtown ramp
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

	// Mad Scientist: its own rules (ft2_mad.cpp)
	if (ft2_mad::Active())
	{
		if (code == MessageCode::ControlCollision && !t->TiltLockFlag && ft2_mad::Collision(caller))
		{
			play_component_sound(caller);
			rs.BonusHits++;
			return;
		}
		if (code != MessageCode::ControlCollision && ft2_mad::Message(code, caller))
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
		if (hb.Battle && hb.BattleTimer) // during a timed battle the scoop restarts its 60 seconds (0x406580)
		{
			start_battle_timer();
			show(upper_copy(hero_text(3033, "Extend Battle")), 2.0f);
			caller->Message(MessageCode::TSinkResetTimer, 1.0f);
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
			points = hero_hit(caller, points);
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
		else if (hb.Battle - 1 == EvilTwin)
		{
			int left = 0;
			for (auto piece : hb.Krypton)
				left += piece;
			if (hb.SuperLit == 1)
				lines.push_back("Evil Twin: downtown ramp " + std::to_string(3 - hb.SuperShots) +
				                " more for the Super Jackpot");
			else if (hb.SuperLit == 2)
				lines.emplace_back("Evil Twin: Super Jackpot collected");
			else
				lines.push_back("Evil Twin: blow up Krypton-@ (" + std::to_string(left) + " left)");
		}
		else if (hb.Battle)
		{
			auto index = hb.Battle - 1;
			auto seconds = std::max(0, (hb.BattleEnds - pb::time_ticks + 999) / 1000);
			lines.push_back("Battle " + villain(index) + ": " + VillainGoals[index] + " (" +
			                std::to_string(VillainGoal[index] - hb.Progress) + " to go, " +
			                std::to_string(seconds) + " s)");
		}
		else
		{
			int left = 0;
			for (int i = 0; i < EvilTwin; i++)
				left += !hb.Defeated[i];
			lines.push_back(left ? "Shoot the scoop to battle a villain (" + std::to_string(left) + " at large)"
			                     : std::string("Shoot the scoop to battle Evil Twin"));
		}
		lines.push_back("Jackpot " + with_commas(hb.Jackpot));
		lines.push_back("Super Jackpot " + with_commas(hb.SuperJackpot) +
		                (hb.SuperMultiplier > 1 ? " x" + std::to_string(hb.SuperMultiplier) : std::string()));
		if (hb.Captured)
			lines.push_back("Villains captured: " + std::to_string(hb.Captured));
		if (any_damage())
			lines.push_back("Buildings standing: " + std::to_string(standing_buildings()) + " of 10 (pops repair)");
		if (hb.MysteryLit)
			lines.emplace_back("Mystery lit: shoot the hole by the pops");
		if (hb.ExtraBallLit)
			lines.emplace_back("Extra ball lit");
		if (rs.SaveSink && rs.SaveSink->ActiveFlag)
			lines.emplace_back("Forcefields on (ball saver open)");
	}
	if (ft2_alien::Active())
	{
		for (auto& line : ft2_alien::Status())
			lines.push_back(line);
		return lines;
	}
	if (ft2_mad::Active())
	{
		for (auto& line : ft2_mad::Status())
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
		if (ft2_mad::Active())
			ft2_mad::NewGame();
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
		if (is_hero())
			set_pling(false);
		if (pb::DevDebug && getenv("FT2_BATTLE") && is_hero()) // development: start a battle at once
		{
			start_battle(std::max(0, std::min(VillainCount - 1, atoi(getenv("FT2_BATTLE")) - 1)));
			if (getenv("FT2_BATTLEPROGRESS"))
				hb.Progress = atoi(getenv("FT2_BATTLEPROGRESS"));
		}
		if (pb::DevDebug && getenv("FT2_OFFER") && is_hero()) // development: the villain choice at once
			offer_battle();
		if (pb::DevDebug && getenv("FT2_HITS")) // development: scripted hits, see scripted_hit
			timer::set(3.0f, nullptr, scripted_hit);
		break;
	case MessageCode::GameOver:
		if (enhanced() && !rs.AttractTimer)
			rs.AttractTimer = timer::set(3.5f, nullptr, attract_tick); // lights play while waiting for a new game
		break;
	case MessageCode::SwitchToNextPlayer:
		raise_blockers(); // blockers drop when the player changes
		if (ft2_alien::Active())
			ft2_alien::PlayerChanged();
		if (ft2_mad::Active())
			ft2_mad::PlayerChanged();
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

void ft2_control::RaisePopValue(bool toMax)
{
	auto rule = table_pop_rule();
	if (!rule)
		return;
	rs.PopValue = toMax ? rule->Max : std::min(rule->Max, std::max(rs.PopValue, rule->Start) + rule->Step);
	rs.PopHits = 0;
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
