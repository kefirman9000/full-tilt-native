#include "pch.h"
#include "ft2_mad.h"

#include "ft2_control.h"
#include "pb.h"
#include "midi.h"
#include "table_art.h"
#include "timer.h"
#include "TBall.h"
#include "TKickout.h"
#include "TLight.h"
#include "TPinballTable.h"
#include "TPlunger.h"
#include "TPopupTarget.h"
#include "TSink.h"
#include "TSound.h"
#include "TTextBox.h"

// Mad Scientist rules, ported from mad.exe. Addresses are the original functions, so each rule can be checked
// against the disassembly (ft2/mad_rules.md has the notes).
namespace
{
	// Experiments (0x430e1c choose, 0x4304c4 start, 0x430a30 end). 0 is none.
	enum Experiment { NoExperiment, Graveyard, AngryMob, StealBrain, Elixir, BringToLife, ExperimentCount };
	const int ExperimentText[ExperimentCount] = {0, 3045, 3046, 3047, 3048, 3049};
	const char* ExperimentNames[ExperimentCount] = {"", "Raid Graveyard for Parts", "Fight Angry Mob!",
	                                                "Steal Brain for Monster", "Make Elixir of Life",
	                                                "Bring Monster to Life"};
	// a lit mode light for each experiment done (0x430e1c)
	const char* ModeLights[ExperimentCount] = {nullptr, "mode_light04", "mode_light05", "mode_light03",
	                                           "mode_light02", "mode_light01"};
	const float ExperimentSeconds = 40.0f; // the last one has no time limit
	const float ExtendSeconds = 10.0f;     // each Igor shot during an experiment

	const char* MonsterDrops[7] = {"monster_drop01", "monster_drop02", "monster_drop03", "monster_drop04",
	                               "monster_drop05", "monster_drop06", "monster_drop07"};
	const char* MonsterLights[7] = {"monster_light01", "monster_light02", "monster_light03", "monster_light04",
	                                "monster_light05", "monster_light06", "monster_light07"};
	const char* MobDrops[9] = {"angrymob_drop01", "angrymob_drop02", "angrymob_drop03", "angrymob_drop04",
	                           "angrymob_drop05", "angrymob_drop06", "angrymob_drop07", "angrymob_drop08",
	                           "angrymob_drop09"};

	// Monster parts (texts 3100-3105): Head, Left Arm, Left Leg from the element D kickout, Torso, Right Arm,
	// Right Leg from the element C ramp; each one lights its part of the body (0x42d0e0)
	const char* PartLights[6] = {"body03a", "body01a", "body04a", "body06a", "body02a", "body05a"};
	const char* PartNames[6] = {"Head", "Left Arm", "Left Leg", "Torso", "Right Arm", "Right Leg"};

	// Brains: four places, four brains each, better ones further on (texts 3200-3215, IQ at 0x4451c4); a brain is
	// worth 2,000 per IQ point
	const int BrainIQ[16] = {5, 10, 15, 20, 35, 45, 55, 65, 75, 85, 95, 105, 140, 160, 180, 200};
	const char* BrainNames[16] = {"Chicken", "Cow", "Sheep", "Pig", "Damaged", "Slightly Used", "Bargain",
	                              "Designer", "Programmer", "Abnormal", "John Doe", "Jane Doe", "Monkey",
	                              "Scientist", "Artificial", "Einstein"};
	const char* PlaceNames[4] = {"Farm", "Morgue", "Joe's Brain and Lube", "Research Facility"};
	const char* PlaceLights[4] = {"mercury_combo_light", "element_b_combo_light", "element_c_combo_light",
	                              "tower_light01"};
	const int PlaceMissScore[4] = {40000, 75000, 75000, 400000}; // a place shot that has no better brain

	// The flask: Einsteinium (element A kickout), Lecithin (element B ramp), Yttrium (element C ramp), Bat Tongue
	// (element D kickout). Mixed, the four bits pick a potion (0x4452e4)
	const char* ElementNames[4] = {"Einsteinium", "Lecithin", "Yttrium", "Bat Tongue"};
	const char* ElementLights[4] = {"element_a_light", "element_b_combo_light", "element_c_combo_light",
	                                "element_d_light"};
	const int PotionOf[16] = {-1, 1, 2, 5, 3, 6, 9, 11, 4, 7, 8, 12, 10, 13, 14, 15};
	const int PotionPoints[6] = {10000, 25000, 50000, 100000, 250000, 500000}; // 0x4452bc
	const int ElixirElementScores[4] = {100000, 150000, 200000, 250000};      // 0x44522c, by elements so far

	const int BRampCombo[4] = {75000, 150000, 225000, 300000};  // 0x4450f8 (50,000 without the combo)
	const int CRampCombo[4] = {80000, 120000, 160000, 200000};  // 0x445110 (40,000)
	const int MercuryCombo[4] = {50000, 100000, 150000, 200000}; // 0x445330 (25,000)
	const int MonsterBank[4] = {15000, 30000, 45000, 60000};     // 0x444f98.., all seven monster targets

	struct PlayerState // the original's per-player block at 0x44a984
	{
		bool Flask[4]{};
		bool MobDone = false, GraveDone = false, ElixirDone = false;
		bool Parts[6]{};
		int BrainValue = 0, BrainIndex = -1;
		int BrainProgress[4]{};
		long long Jackpot = 1000000;
		long long HeldBonus = 0;
		int PotionStep = 0;
	};

	struct MadState
	{
		TPinballTable* Table = nullptr;
		std::map<std::string, TPinballComponent*> Parts;
		PlayerState Players[4];

		int Mode = NoExperiment, NextMode = NoExperiment;
		float ModeLeft = 0;
		int ModeTimer = 0;
		int Wizard = 0; // Bring Monster to Life: 1 all seven parts charged, 2 alive (0x444f94 / 0x44a9d4)
		int WizardTimer = 0;
		int MobHits = 0;
		bool MonsterHit[7]{};
		int MonsterBankStep = 0;
		int Multiplier = 1;
		bool TeleporterOn = false, CloningLit = false, ExtraBallLit = false, ShootAgain = false;
		bool RecaptureLeft = false, RecaptureRight = false, RecaptureArmed = false, RecaptureSave = false;
		int PlasmaDown = 0, PlasmaStep = 0; // plasma field on while PlasmaStep > 0
		int BCombo = 0, CCombo = 0, MercuryCombo = 0;
		int BComboUntil = 0, CComboUntil = 0, MercuryComboUntil = 0;
		bool SkillShot = false;
		bool LaunchSave = true;                 // 0x444f60: a new ball, its shoot again still to come
		int SaveTimer = 0, SaveState = 2;       // 0x444f58 / 0x444f5c: 0 steady, 1 flashing, 2 off
		TKickout* FlaskHolding = nullptr;
		int FlaskTimer = 0, ResetTimer = 0;

		// this ball's bonus (0x444f28..0x444f30)
		int Potions = 0, Experiments = 0, MonsterParts = 0;
	} ms;

	TPinballTable* table()
	{
		return pb::MainTable;
	}

	PlayerState& player()
	{
		auto index = table() ? table()->CurrentPlayer : 0;
		return ms.Players[std::max(0, std::min(3, index))];
	}

	TPinballComponent* part(const char* name)
	{
		auto found = ms.Parts.find(name);
		if (found != ms.Parts.end())
			return found->second;
		TPinballComponent* component = nullptr;
		for (auto candidate : table()->ComponentList)
			if (candidate->GroupName && strcmp(candidate->GroupName, name) == 0)
			{
				component = candidate;
				break;
			}
		ms.Parts[name] = component;
		if (!component && pb::DevDebug)
			printf("FT2DBG mad: no part named %s\n", name);
		return component;
	}

	bool is(TPinballComponent* caller, const char* name)
	{
		return caller && caller->GroupName && strcmp(caller->GroupName, name) == 0;
	}

	int index_of(TPinballComponent* caller, const char* const* names, int count)
	{
		for (int i = 0; i < count; i++)
			if (is(caller, names[i]))
				return i;
		return -1;
	}

	void send(const char* name, MessageCode code, float value = 0.0f)
	{
		if (auto component = part(name))
			component->Message(code, value);
	}

	void light(const char* name, bool on)
	{
		send(name, on ? MessageCode::TLightResetAndTurnOn : MessageCode::TLightResetAndTurnOff);
	}

	void flash(const char* name)
	{
		send(name, MessageCode::TLightFlasherStart);
	}

	void wave(const char* name)
	{
		if (auto sound = dynamic_cast<TSound*>(part(name)))
			sound->Play(nullptr, name);
	}

	void kill(int& timerId)
	{
		if (timerId)
			timer::kill(timerId);
		timerId = 0;
	}

	// A text of mad.exe, cleaned for display: '>' is an apostrophe, '`' and '^' are padding and icons
	std::string text(int id, const std::string& fallback)
	{
		auto raw = table_art::Text(id, fallback);
		std::string out;
		for (auto c : raw)
		{
			if (c == '>')
				out += '\'';
			else if (c == '\n')
				out += ' ';
			else if (c != '`' && c != '^' && c != '[')
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

	std::string commas(long long value)
	{
		auto digits = std::to_string(value);
		for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
			digits.insert(i, ",");
		return digits;
	}

	std::string format(int id, const std::string& fallback, long long value)
	{
		auto pattern = text(id, fallback);
		auto at = pattern.find("%ld");
		if (at != std::string::npos)
			pattern.replace(at, 3, commas(value));
		return pattern;
	}

	void show(const std::string& message, float seconds = 3.0f)
	{
		if (pb::DevDebug)
			printf("FT2DBG mad: %s\n", message.c_str());
		if (pb::InfoTextBox)
			pb::InfoTextBox->Display(message.c_str(), seconds);
	}

	void show_mission(const std::string& message)
	{
		if (pb::DevDebug)
			printf("FT2DBG mad mission: %s\n", message.c_str());
		if (pb::MissTextBox && pb::MissTextBox != pb::InfoTextBox)
			pb::MissTextBox->Display(message.c_str(), -1.0f);
		else
			show(message, 4.0f);
	}

	// 0x42d41c: points, times the table multiplier
	void award(long long points)
	{
		if (table() && !table()->TiltLockFlag && points > 0)
			table()->AddScore(static_cast<int>(points * ms.Multiplier));
	}

	void add_jackpot(long long points)
	{
		auto& jackpot = player().Jackpot;
		jackpot = std::min<long long>(10000000, jackpot + points);
	}

	bool single_ball()
	{
		return table()->MultiballCount <= 1;
	}

	void eject(TPinballComponent* pocket, float seconds = 1.0f)
	{
		if (dynamic_cast<TKickout*>(pocket))
			pocket->Message(MessageCode::TKickoutRestartTimer, seconds);
		else if (dynamic_cast<TSink*>(pocket))
			pocket->Message(MessageCode::TSinkResetTimer, seconds);
	}

	void raise(const char* name)
	{
		if (auto target = dynamic_cast<TPopupTarget*>(part(name)))
			if (!target->ActiveFlag && !target->Timer)
				target->Message(MessageCode::TPopupTargetEnable, 0.0f);
	}

	void lower(const char* name)
	{
		if (auto target = dynamic_cast<TPopupTarget*>(part(name)))
		{
			if (target->Timer)
			{
				timer::kill(target->Timer);
				target->Timer = 0;
			}
			target->Message(MessageCode::TPopupTargetDisable, 0.0f);
		}
	}

	// A ball that went somewhere it does not come back from (into the flask): a new one comes to the plunger
	void ball_taken()
	{
		auto t = table();
		t->MultiballCount = std::max(0, t->MultiballCount - 1);
		if (t->MultiballCount == 0 && t->Plunger)
			t->Plunger->Message(MessageCode::PlungerStartFeedTimer, 0.0f);
	}

	void add_ball_at(const char* sinkName)
	{
		auto exit = dynamic_cast<TSink*>(part(sinkName));
		auto t = table();
		if (!exit)
			return;
		if (auto ball = t->AddBall(exit->BallPosition))
		{
			ball->throw_ball(&exit->BallThrowDirection, exit->ThrowAngleMult, exit->ThrowSpeedMult1,
			                 exit->ThrowSpeedMult2);
			t->MultiballCount++;
		}
	}

	// 0x42cf84: the two teleporter diverters on the element B ramp, one up and one down. With the teleporter lit
	// the ramp takes the ball to the teleporter, otherwise round to its hole
	void update_diverters()
	{
		bool teleport = ms.TeleporterOn;
		auto a = part("teleporter_diverter_a"), b = part("teleporter_diverter_b");
		if (a)
			a->Message(teleport ? MessageCode::TGateEnable : MessageCode::TGateDisable, 0.0f);
		if (b)
			b->Message(teleport ? MessageCode::TGateDisable : MessageCode::TGateEnable, 0.0f);
	}

	// The teleporter (0x42efc0): the ball that went into teleporter_sink comes out of teleporter_sink2 at the top
	// left. The sink's own exit is a dead end, so the ball is served at the other one instead.
	void teleport(int, void*)
	{
		auto exit = dynamic_cast<TSink*>(part("teleporter_sink2"));
		if (!exit)
			return;
		if (auto ball = table()->AddBall(exit->BallPosition))
		{
			ball->throw_ball(&exit->BallThrowDirection, exit->ThrowAngleMult, exit->ThrowSpeedMult1,
			                 exit->ThrowSpeedMult2);
			// it comes out at the top of the teleport exit ramp, on the ramp's level (teleporter_sink2's group)
			ball->CollisionMask = 2;
		}
	}

	void teleport(TPinballComponent* sink)
	{
		if (part("teleporter_sink2"))
			timer::set(0.5f, nullptr, teleport); // the ball in teleporter_sink stays there (disabled)
		else
			eject(sink);
	}

	// ---- lights that show the state ----
	void update_flask()
	{
		auto& p = player();
		int count = p.Flask[0] + p.Flask[1] + p.Flask[2] + p.Flask[3];
		light("flask_kickout_light", count > 0);
	}

	void update_body()
	{
		auto& p = player();
		for (int i = 0; i < 6; i++)
			light(PartLights[i], p.Parts[i]);
	}

	void update_multiplier()
	{
		static const char* lights[4] = {"table_multiplier_light01", "table_multiplier_light02",
		                                "table_multiplier_light03", "table_multiplier_light04"};
		for (int i = 0; i < 4; i++)
			light(lights[i], ms.Multiplier >= i + 2);
	}

	void update_mode_lights()
	{
		auto& p = player();
		bool done[ExperimentCount] = {false, p.GraveDone, p.MobDone, p.BrainValue > 0, p.ElixirDone, false};
		for (int i = 1; i < ExperimentCount; i++)
			light(ModeLights[i], done[i]);
		if (ms.Mode)
			flash(ModeLights[ms.Mode]);
		else if (ms.NextMode)
			flash(ModeLights[ms.NextMode]);
		light("igor_light", ms.NextMode && !ms.Mode);
	}

	void update_monster_lights()
	{
		for (int i = 0; i < 7; i++)
			light(MonsterLights[i], ms.MonsterHit[i]);
	}

	// 0x430e1c: the next experiment, at random from those not done yet. Bring Monster to Life comes once the parts,
	// a brain and the elixir are in hand; fighting the mob is not needed for it
	void choose_next()
	{
		auto& p = player();
		std::vector<int> open;
		if (!p.GraveDone)
			open.push_back(Graveyard);
		if (!p.MobDone)
			open.push_back(AngryMob);
		if (p.BrainValue <= 0)
			open.push_back(StealBrain);
		if (!p.ElixirDone)
			open.push_back(Elixir);
		if (p.GraveDone && p.BrainValue > 0 && p.ElixirDone)
			open = {BringToLife};
		ms.NextMode = open.empty() ? NoExperiment : open[rand() % open.size()];
		update_mode_lights();
		if (ms.NextMode)
			show_mission(text(3003, "Begin Experiment") + ": " +
				text(ExperimentText[ms.NextMode], ExperimentNames[ms.NextMode]));
	}

	void reset_monster_bank(int = 0, void* = nullptr)
	{
		ms.ResetTimer = 0;
		for (int i = 0; i < 7; i++)
		{
			ms.MonsterHit[i] = false;
			raise(MonsterDrops[i]);
		}
		update_monster_lights();
	}

	void experiment_over(bool success);

	void show_countdown()
	{
		if (!ms.Mode)
			return;
		std::string line = ExperimentNames[ms.Mode];
		if (ms.Mode != BringToLife)
			line += "  " + std::to_string(static_cast<int>(ms.ModeLeft + 0.5f));
		show_mission(line);
	}

	void experiment_tick(int, void*)
	{
		ms.ModeTimer = 0;
		if (!ms.Mode)
			return;
		ms.ModeLeft -= 1.0f;
		if (ms.ModeLeft <= 0)
		{
			auto& p = player();
			bool success = (ms.Mode == Graveyard && p.GraveDone) || (ms.Mode == AngryMob && p.MobDone) ||
				(ms.Mode == StealBrain && p.BrainValue > 0) || (ms.Mode == Elixir && p.ElixirDone);
			experiment_over(success);
			return;
		}
		show_countdown();
		ms.ModeTimer = timer::set(1.0f, nullptr, experiment_tick);
	}

	// 0x4304c4: Igor starts the experiment picked for the next one
	void start_experiment()
	{
		ms.Mode = ms.NextMode;
		midi::play_ft2_track(ms.Mode + 1); // 0x4304c4: each experiment has its music, tabd2-tabd6
		ms.ModeLeft = ExperimentSeconds;
		auto& p = player();
		wave("wave42");
		show(text(ExperimentText[ms.Mode], ExperimentNames[ms.Mode]), 4.0f);
		switch (ms.Mode)
		{
		case Graveyard: // body parts at the element D kickout (left) and the element C ramp (right)
			flash("element_d_light");
			flash("element_c_combo_light");
			break;
		case AngryMob: // 0x42e89c: the mob comes up, the monster targets go down
			ms.MobHits = 0;
			for (auto name : MonsterDrops)
				lower(name);
			for (auto name : MobDrops)
				raise(name);
			break;
		case StealBrain: // the four places with a better brain flash
			for (int place = 0; place < 4; place++)
				if (p.BrainProgress[place] < 4)
					flash(PlaceLights[place]);
			break;
		case Elixir:
			for (int i = 0; i < 4; i++)
				if (!p.Flask[i])
					flash(ElementLights[i]);
			break;
		case BringToLife: // 0x42e0bc: charge the seven monster targets
			ms.Wizard = 0;
			reset_monster_bank();
			show(text(3400, "Assemble Monster"), 4.0f);
			break;
		default:
			break;
		}
		kill(ms.ModeTimer);
		if (ms.Mode != BringToLife)
			ms.ModeTimer = timer::set(1.0f, nullptr, experiment_tick);
		update_mode_lights();
		show_countdown();
	}

	// 0x430a30
	void experiment_over(bool success)
	{
		auto mode = ms.Mode;
		if (!mode)
			return;
		kill(ms.ModeTimer);
		kill(ms.WizardTimer);
		ms.Mode = NoExperiment;
		midi::play_ft2_track(1); // no experiment: tabd1
		auto& p = player();
		switch (mode)
		{
		case AngryMob:
			for (auto name : MobDrops)
				lower(name);
			reset_monster_bank();
			break;
		case BringToLife:
			if (ms.Wizard == 2) // the monster lives: everything starts over (0x42ceb8)
			{
				auto jackpot = p.Jackpot;
				auto held = p.HeldBonus;
				p = PlayerState{};
				p.Jackpot = jackpot;
				p.HeldBonus = held;
				update_flask();
				update_body();
			}
			ms.Wizard = 0;
			light("tower_light01", false);
			light("tower_light02", false);
			reset_monster_bank();
			break;
		default:
			break;
		}
		for (int place = 0; place < 4; place++)
			light(PlaceLights[place], false);
		light("element_a_light", false);
		light("element_d_light", false);
		if (success)
		{
			ms.Experiments++;
			show(std::string(ExperimentNames[mode]) + "  COMPLETE", 4.0f);
		}
		else
			show(std::string(ExperimentNames[mode]) + "  OVER", 3.0f);
		if (pb::MissTextBox && pb::MissTextBox != pb::InfoTextBox)
			pb::MissTextBox->Clear();
		choose_next();
	}

	// ---- Bring Monster to Life (0x42e0ec in mode 5, 0x42f118 tower sink) ----
	void wizard_done(int, void*)
	{
		ms.WizardTimer = 0;
		experiment_over(true);
	}

	// ---- Steal a brain (0x431570) ----
	void brain_shot(int place)
	{
		auto& p = player();
		if (p.BrainProgress[place] < 4)
		{
			auto index = place * 4 + p.BrainProgress[place];
			if (BrainIQ[index] > p.BrainValue)
			{
				p.BrainProgress[place]++;
				p.BrainValue = BrainIQ[index];
				p.BrainIndex = index;
				award(2000LL * BrainIQ[index]);
				show(std::string(PlaceNames[place]) + ": " + text(3200 + index, BrainNames[index]) + " brain", 3.0f);
				wave("wave44");
				if (p.BrainProgress[place] >= 4)
					light(PlaceLights[place], false);
				update_mode_lights();
				return;
			}
		}
		award(PlaceMissScore[place]);
	}

	// ---- Monster parts (0x431b94 left, 0x431cfc right) ----
	bool take_part(bool left)
	{
		auto& p = player();
		int first = left ? 0 : 3;
		for (int i = first; i < first + 3; i++)
			if (!p.Parts[i])
			{
				p.Parts[i] = true;
				ms.MonsterParts++;
				award(75000);
				show(text(3100 + i, PartNames[i]), 2.5f);
				wave("wave45");
				update_body();
				bool all = true;
				for (auto got : p.Parts)
					all = all && got;
				if (all)
				{
					p.GraveDone = true;
					experiment_over(true);
				}
				return true;
			}
		return false;
	}

	// ---- The flask (0x431ca4 elements, 0x42fcb0 mix or empty, 0x4324bc potion) ----
	void add_element(int element, TPinballComponent* where)
	{
		auto& p = player();
		if (p.Flask[element])
			return;
		int count = p.Flask[0] + p.Flask[1] + p.Flask[2] + p.Flask[3];
		p.Flask[element] = true;
		if (ms.Mode == Elixir)
		{
			award(ElixirElementScores[std::min(3, count)]);
			light(ElementLights[element], false);
			if (count + 1 >= 4)
			{
				flash("flask_kickout_light");
				show(text(3304, "Mix Elixir of Life"), 4.0f);
			}
		}
		show(text(3006 + element, ElementNames[element]), 2.5f);
		(void)where;
		update_flask();
		if (ms.Mode == Elixir && count + 1 >= 4)
			flash("flask_kickout_light");
	}

	void potion(int which)
	{
		auto t = table();
		ms.Potions++;
		switch (which)
		{
		case 1:
			award(PotionPoints[player().PotionStep]);
			player().PotionStep = std::min(5, player().PotionStep + 1);
			show(text(3021, "Points Awarded!"));
			break;
		case 2:
			add_jackpot(500000);
			show(text(3022, "Jackpot Increased!"));
			break;
		case 3:
			ms.TeleporterOn = true;
			update_diverters();
			show(text(3023, "Teleporter Active!"));
			break;
		case 4:
			ft2_control::RaisePopValue(false);
			show(text(3024, "Bumpers Increased!"));
			break;
		case 5:
			ms.ShootAgain = true;
			light("shoot_again_light", true);
			show(text(3025, "Shoot Again Light On!"));
			break;
		case 6:
			ms.CloningLit = true;
			light("cloning_light", true);
			show(text(3026, "Cloning Light On!"));
			break;
		case 7:
			ft2_control::RaisePopValue(true);
			show(text(3027, "Max Bumpers!"));
			break;
		case 8:
			ms.PlasmaStep = 1;
			light("plasma_light01", true);
			light("plasma_light02", true);
			show(text(3028, "Plasma Field Activated!"));
			break;
		case 9:
			ms.Multiplier = std::min(5, ms.Multiplier + 1);
			update_multiplier();
			show(text(3029, "Table Multiplier Increased!"));
			break;
		case 10:
			award(500000);
			show(text(3030, "500K POINTS!!!"));
			break;
		case 11:
			ms.ExtraBallLit = true;
			flash("extra_ball_light");
			show(text(3031, "Extra Ball Light On!"));
			break;
		case 12:
			award(1000000);
			show(text(3032, "1 MILLION POINTS!!!"));
			break;
		case 13:
			show(text(3033, "MULTI-BALL!!!"));
			add_ball_at("Hg_exit_sink_01");
			add_ball_at("Hg_exit_sink_02");
			break;
		case 14:
			ms.Multiplier = 5;
			update_multiplier();
			show(text(3034, "Table Multiplier 5X!!!"));
			break;
		case 15:
			t->ExtraBalls++;
			show(text(3035, "EXTRA BALL!!!"));
			break;
		default:
			break;
		}
		wave("wave46");
	}

	// The ball goes into the flask (the original kicks it into flask_sink): the contents turn into a potion, or the
	// Elixir of Life during that experiment; a new ball comes to the plunger
	void mix()
	{
		kill(ms.FlaskTimer);
		auto kickout = ms.FlaskHolding;
		ms.FlaskHolding = nullptr;
		auto& p = player();
		int mask = p.Flask[0] | p.Flask[1] << 1 | p.Flask[2] << 2 | p.Flask[3] << 3;
		for (auto& element : p.Flask)
			element = false;
		update_flask();
		if (kickout && kickout->BallCaputeredFlag && kickout->Ball)
		{
			kickout->Ball->Disable();
			kickout->Ball = nullptr;
			kickout->BallCaputeredFlag = 0;
			ball_taken();
		}
		else if (kickout)
			eject(kickout);
		if (ms.Mode == Elixir && mask == 15)
		{
			p.ElixirDone = true;
			award(500000);
			ms.Potions++;
			show(text(3048, "Make Elixir of Life") + "  COMPLETE", 4.0f);
			experiment_over(true);
			return;
		}
		if (mask && PotionOf[mask] > 0)
			potion(PotionOf[mask]);
	}

	void empty_flask()
	{
		kill(ms.FlaskTimer);
		auto& p = player();
		int count = p.Flask[0] + p.Flask[1] + p.Flask[2] + p.Flask[3];
		for (auto& element : p.Flask)
			element = false;
		update_flask();
		award(50000LL * count);
		show(text(3020, "Empty"), 2.0f);
		if (ms.FlaskHolding)
			eject(ms.FlaskHolding);
		ms.FlaskHolding = nullptr;
	}

	// 0x42da48 / 0x42d994: the shoot again light, steady and then flashing for its last 5 seconds; a drain while
	// it is on gives the ball back (0x42da10 puts it out)
	void timed_save_step(int, void*)
	{
		ms.SaveTimer = 0;
		if (ms.SaveState == 0)
		{
			ms.SaveState = 1;
			flash("shoot_again_light");
			ms.SaveTimer = timer::set(5.0f, nullptr, timed_save_step);
		}
		else
		{
			ms.SaveState = 2;
			light("shoot_again_light", ms.ShootAgain);
		}
	}

	void timed_save(float seconds)
	{
		kill(ms.SaveTimer);
		ms.SaveState = 0;
		light("shoot_again_light", true);
		ms.SaveTimer = timer::set(std::max(0.1f, seconds - 5.0f), nullptr, timed_save_step);
	}

	void timed_save_off()
	{
		kill(ms.SaveTimer);
		ms.SaveState = 2;
		light("shoot_again_light", ms.ShootAgain);
	}

	void flask_timeout(int, void*)
	{
		ms.FlaskTimer = 0;
		if (ms.FlaskHolding)
			mix();
	}

	std::string flask_contents()
	{
		auto& p = player();
		std::string line = text(3015, "Flask Contains:");
		bool any = false;
		for (int i = 0; i < 4; i++)
			if (p.Flask[i])
			{
				line += std::string(any ? ", " : " ") + ElementNames[i];
				any = true;
			}
		if (!any)
			line += " " + text(3020, "Empty");
		return line;
	}

	void reset_ball_state()
	{
		ms.Potions = ms.Experiments = ms.MonsterParts = 0;
	}
}

bool ft2_mad::Active()
{
	if (!table())
		return false;
	std::string key = pb::DatFileName;
	auto slash = key.find_last_of("/\\");
	if (slash != std::string::npos)
		key = key.substr(slash + 1);
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	if (key != "MAD.DAT")
		return false;
	if (ms.Table != table())
	{
		ms = MadState{};
		ms.Table = table();
	}
	return true;
}

void ft2_mad::NewGame()
{
	auto t = ms.Table;
	kill(ms.ModeTimer);
	kill(ms.FlaskTimer);
	kill(ms.WizardTimer);
	kill(ms.ResetTimer);
	kill(ms.SaveTimer);
	ms = MadState{};
	ms.Table = t;
	for (auto name : MobDrops)
		lower(name);
	reset_monster_bank();
	for (auto name : {"cloning_light", "shoot_again_light", "extra_ball_light", "plasma_light01", "plasma_light02",
	                  "recapture_activation_light01", "recapture_activation_light02", "recapture_ball_light",
	                  "ball_recapture_activated_light01", "ball_recapture_activated_light02", "tower_light01",
	                  "tower_light02", "element_a_light", "element_d_light", "mercury_combo_light",
	                  "element_b_combo_light", "element_c_combo_light"})
		light(name, false);
	update_flask();
	update_body();
	update_multiplier();
	update_diverters();
	choose_next();
}

void ft2_mad::PlayerChanged()
{
	if (ms.Mode)
		experiment_over(false);
	ms.Multiplier = 1;
	update_multiplier();
	update_flask();
	update_body();
	choose_next();
}

// 0x432eec: the skill shot is the first thing the launched ball hits
void ft2_mad::Launch()
{
	ms.SkillShot = true;
	for (int i = 0; i < 7; i++)
		if (!ms.MonsterHit[i])
			flash(MonsterLights[i]);
}

bool ft2_mad::SaveBall()
{
	if (ms.RecaptureSave)
	{
		ms.RecaptureSave = false;
		show("BALL RECAPTURED", 2.5f);
		return true;
	}
	if (ms.SaveState != 2)
	{
		timed_save_off();
		show("SHOOT AGAIN", 2.5f);
		return true;
	}
	if (ms.ShootAgain)
	{
		ms.ShootAgain = false;
		light("shoot_again_light", false);
		show("SHOOT AGAIN", 2.5f);
		return true;
	}
	return false;
}

// 0x4300d8: potions x 50,000, experiments x 250,000, monster parts x 100,000, plus the last ball's bonus again
void ft2_mad::EndOfBall()
{
	auto& p = player();
	if (ms.Mode && ms.Mode != BringToLife)
	{
		bool success = (ms.Mode == Graveyard && p.GraveDone) || (ms.Mode == AngryMob && p.MobDone) ||
			(ms.Mode == StealBrain && p.BrainValue > 0) || (ms.Mode == Elixir && p.ElixirDone);
		experiment_over(success);
	}
	auto t = table();
	long long bonus = ms.Potions * 50000LL + ms.Experiments * 250000LL + ms.MonsterParts * 100000LL;
	auto total = bonus + p.HeldBonus;
	if (total > 0 && !t->TiltLockFlag)
	{
		t->AddScore(static_cast<int>(total));
		std::string summary = format(3037, "Potions %ld", ms.Potions * 50000LL) + "  " +
			format(3038, "Experiments %ld", ms.Experiments * 250000LL) + "  " +
			format(3039, "Monster Parts %ld", ms.MonsterParts * 100000LL) + "  " +
			(p.HeldBonus ? format(3040, "Bonus Hold %ld", p.HeldBonus) + "  " : std::string()) +
			format(3503, "Total Bonus %ld", total);
		if (pb::InfoTextBox)
			pb::InfoTextBox->Display(summary.c_str(), 6.0f);
	}
	p.HeldBonus = bonus;
	ms.Multiplier = 1;
	update_multiplier();
	ms.SkillShot = false;
	ms.RecaptureArmed = ms.RecaptureLeft = ms.RecaptureRight = false;
	light("recapture_ball_light", false);
	light("recapture_activation_light01", false);
	light("recapture_activation_light02", false);
	reset_ball_state();
	timed_save_off();
	ms.LaunchSave = true;
}

bool ft2_mad::Message(MessageCode code, TPinballComponent* caller)
{
	if (code != MessageCode::TFlipperExtend || !ms.FlaskHolding)
		return false;
	bool left = is(caller, "left_flipper") || is(caller, "flpr01");
	bool right = is(caller, "right_flipper") || is(caller, "flpr02");
	if (left)
		mix();
	else if (right)
		empty_flask();
	else
		return false;
	return true;
}

bool ft2_mad::Collision(TPinballComponent* caller)
{
	auto t = table();
	if (!caller || t->TiltLockFlag)
		return false;
	if (pb::DevDebug)
		printf("FT2DBG mad hit %s\n", caller->GroupName ? caller->GroupName : "?");
	auto& p = player();

	// the shooter lane's one-way (0x42dab8): the first time each new ball goes through, 15 s of shoot again
	if (is(caller, "plunger_oneway") && ms.LaunchSave)
	{
		ms.LaunchSave = false;
		timed_save(15.0f);
	}

	// the skill shot (0x432eec): the first part the launched ball hits, other than the shooter lane
	if (ms.SkillShot && !is(caller, "plunger") && !is(caller, "plunger_oneway"))
	{
		ms.SkillShot = false;
		update_monster_lights();
		auto drop = index_of(caller, MonsterDrops, 7);
		static const int skill[7] = {1000000, 0, 500000, 100000, 50000, 50000, 50000};
		if (drop >= 0)
		{
			show("SKILL SHOT", 2.5f);
			award(skill[drop]);
			if (drop == 0)
			{
				ms.PlasmaStep = 1;
				light("plasma_light01", true);
				light("plasma_light02", true);
				show(text(3028, "Plasma Field Activated!"));
			}
			if (drop == 1 && !ms.Mode && ms.NextMode)
				start_experiment();
		}
		else
			award(10000);
	}

	// monster targets (0x42e0ec)
	auto drop = index_of(caller, MonsterDrops, 7);
	if (drop >= 0)
	{
		if (ms.MonsterHit[drop])
			return true;
		ms.MonsterHit[drop] = true;
		light(MonsterLights[drop], true);
		bool all = true;
		for (auto hit : ms.MonsterHit)
			all = all && hit;
		if (ms.Mode == BringToLife)
		{
			award(100000);
			wave("wave47");
			if (all && ms.Wizard == 0)
			{
				ms.Wizard = 1;
				flash("tower_light01");
				flash("tower_light02");
				show(text(3401, "Bring Monster to Life"), 5.0f);
				show_mission(text(3401, "Bring Monster to Life") + " (Tower Sink)");
			}
			return true;
		}
		award(15000);
		if (all)
		{
			award(MonsterBank[ms.MonsterBankStep]);
			ms.MonsterBankStep = std::min(3, ms.MonsterBankStep + 1);
			ms.TeleporterOn = true;
			update_diverters();
			show(text(3001, "Teleporter Active"), 2.5f);
			kill(ms.ResetTimer);
			ms.ResetTimer = timer::set(0.5f, nullptr, reset_monster_bank);
		}
		return true;
	}

	// the angry mob (0x42eb64): 50,000 + 10,000 per villager so far; five is enough, all nine ends it
	auto mob = index_of(caller, MobDrops, 9);
	if (mob >= 0)
	{
		if (ms.Mode != AngryMob)
			return true;
		award(50000 + 10000LL * ms.MobHits);
		ms.MobHits++;
		wave("wave48");
		if (ms.MobHits >= 5 && !p.MobDone)
		{
			p.MobDone = true;
			update_mode_lights();
		}
		if (ms.MobHits >= 9)
			experiment_over(true);
		return true;
	}

	// plasma targets (0x42e764): both down switch the plasma field on; then the element B ramp, the element C ramp
	// and the mercury ramp are worth 250,000 each in turn
	if (is(caller, "plasma_drop01") || is(caller, "plasma_drop02"))
	{
		ms.PlasmaDown++;
		if (ms.PlasmaDown >= 2)
		{
			award(150000);
			ms.PlasmaStep = 1;
			light("plasma_light01", true);
			light("plasma_light02", true);
			show(text(3028, "Plasma Field Activated!"));
		}
		else
		{
			award(25000);
			show(text(3002, "Targets Activate Plasma Field"), 2.5f);
		}
		return true;
	}

	auto plasma_step = [&](int step)
	{
		if (ms.PlasmaStep != step)
			return;
		award(250000);
		wave("wave49");
		if (++ms.PlasmaStep > 3)
		{
			ms.PlasmaStep = 0;
			ms.PlasmaDown = 0;
			light("plasma_light01", false);
			light("plasma_light02", false);
			raise("plasma_drop01");
			raise("plasma_drop02");
		}
	};

	auto now = pb::time_ticks;
	// element B ramp (0x42f594): Lecithin, the Morgue, a combo when shot again within 10 seconds
	if (is(caller, "element_b_ramp"))
	{
		if (ms.Mode == StealBrain)
			brain_shot(1);
		else
		{
			if (now < ms.BComboUntil)
			{
				award(BRampCombo[ms.BCombo]);
				ms.BCombo = std::min(3, ms.BCombo + 1);
			}
			else
			{
				award(50000);
				ms.BCombo = 0;
			}
			ms.BComboUntil = now + 10000;
			send("element_b_combo_light", MessageCode::TLightFlasherStart);
			if (ms.Mode == NoExperiment || ms.Mode == Elixir)
				add_element(1, caller);
		}
		plasma_step(1);
		return true;
	}
	// element C ramp (0x42f7bc): Yttrium, Joe's brains, the right-hand monster parts
	if (is(caller, "element_c_ramp"))
	{
		if (ms.Mode == StealBrain)
			brain_shot(2);
		else if (ms.Mode == Graveyard && take_part(false))
		{
		}
		else
		{
			if (now < ms.CComboUntil)
			{
				award(CRampCombo[ms.CCombo]);
				ms.CCombo = std::min(3, ms.CCombo + 1);
			}
			else
			{
				award(40000);
				ms.CCombo = 0;
			}
			ms.CComboUntil = now + 10000;
			send("element_c_combo_light", MessageCode::TLightFlasherStart);
			if (ms.Mode == NoExperiment || ms.Mode == Elixir)
				add_element(2, caller);
		}
		plasma_step(2);
		return true;
	}
	// mercury ramp (0x42f52c): the Farm's brains; the mercury input sink behind it scores the combo (0x432774)
	if (is(caller, "mercury_ramp"))
	{
		award(10000);
		return true;
	}
	if (is(caller, "mercury_input_sink"))
	{
		plasma_step(3);
		if (ms.Mode == StealBrain)
			brain_shot(0);
		else if (now < ms.MercuryComboUntil)
		{
			award(MercuryCombo[ms.MercuryCombo]);
			ms.MercuryCombo = std::min(3, ms.MercuryCombo + 1);
		}
		else
		{
			award(25000);
			ms.MercuryCombo = 0;
		}
		ms.MercuryComboUntil = now + 10000;
		send("mercury_combo_light", MessageCode::TLightFlasherStart);
		if (ms.CloningLit && t->MultiballCount < 4) // 0x43267c: the ball comes out of the mercury tubes twice
		{
			ms.CloningLit = false;
			light("cloning_light", false);
			show(text(3014, "Clone Ball!"), 2.5f);
			add_ball_at("Hg_exit_sink_02");
		}
		eject(caller);
		return true;
	}

	// element A kickout (0x42fa14): Einsteinium, and the extra ball when lit
	if (is(caller, "element_a_kickout"))
	{
		award(40000);
		if (ms.ExtraBallLit)
		{
			ms.ExtraBallLit = false;
			light("extra_ball_light", false);
			t->ExtraBalls++;
			show(text(3011, "EXTRA BALL!!!"), 2.5f);
		}
		if (ms.Mode == NoExperiment || ms.Mode == Elixir)
			add_element(0, caller);
		eject(caller, 1.25f);
		return true;
	}
	// element D kickout (0x42fb50): Bat Tongue, or the left-hand monster parts during the graveyard raid
	if (is(caller, "element_d_kickout"))
	{
		if (!(ms.Mode == Graveyard && take_part(true)))
		{
			award(40000);
			if (ms.Mode == NoExperiment || ms.Mode == Elixir)
				add_element(3, caller);
		}
		eject(caller, 1.25f);
		return true;
	}
	// flask kickout (0x42fe30): with something in the flask it waits for a flipper: mix (left) or empty (right)
	if (is(caller, "flask_kickout"))
	{
		int count = p.Flask[0] + p.Flask[1] + p.Flask[2] + p.Flask[3];
		auto kickout = dynamic_cast<TKickout*>(caller);
		if (count && kickout && !ms.FlaskHolding)
		{
			ms.FlaskHolding = kickout;
			kickout->Message(MessageCode::TKickoutRestartTimer, 60.0f); // held until a flipper decides
			show(flask_contents() + "  " + text(3013, "Mix Elements (Left Flipper) or Empty Flask (Right Flipper)?"),
			     6.0f);
			kill(ms.FlaskTimer);
			ms.FlaskTimer = timer::set(6.0f, nullptr, flask_timeout);
			return true;
		}
		award(40000);
		eject(caller);
		return true;
	}
	if (is(caller, "flask_sink"))
	{
		award(10000);
		eject(caller);
		return true;
	}

	// Igor (0x42edc8): starts the next experiment in single-ball play, or adds 10 seconds to the one running
	if (is(caller, "igor_sink"))
	{
		if (ms.Mode && ms.Mode != BringToLife)
		{
			ms.ModeLeft += ExtendSeconds;
			award(50000);
			show(text(3004, "Extend Experiment") + "  +10", 2.5f);
			show_countdown();
		}
		else if (!ms.Mode && ms.NextMode && single_ball())
		{
			award(150000);
			start_experiment();
		}
		else
			award(25000);
		eject(caller);
		return true;
	}

	// the tower sink (0x42f118): the lightning rod, the Research Facility's brains, and the monster comes to life
	if (is(caller, "tower_sink"))
	{
		if (ms.Mode == StealBrain)
			brain_shot(3);
		else if (ms.Mode == BringToLife && ms.Wizard == 1)
		{
			ms.Wizard = 2;
			award(p.Jackpot);
			show("IT'S ALIVE!  JACKPOT " + commas(p.Jackpot * ms.Multiplier), 5.0f);
			p.Jackpot = 1000000;
			wave("wave50");
			kill(ms.WizardTimer);
			ms.WizardTimer = timer::set(20.0f, nullptr, wizard_done);
		}
		else
		{
			award(50000);
			show(text(3012, "Lightning Rod Activated!"), 2.5f);
			wave("wave43");
		}
		eject(caller);
		return true;
	}

	// the teleporter (0x42efc0): lit by the monster targets; the teleported ball lights the cloning light
	if (is(caller, "teleporter_sink"))
	{
		if (ms.TeleporterOn)
		{
			ms.TeleporterOn = false;
			update_diverters();
			award(150000);
			show(text(3005, "Ball Teleported"), 2.5f);
			if (!ms.CloningLit)
			{
				ms.CloningLit = true;
				light("cloning_light", true);
			}
		}
		else
			award(25000);
		teleport(caller);
		return true;
	}

	// lanes (0x42f330..0x42f478): both return lanes light the ball recapture; an outlane while it is lit gives the
	// ball back
	if (is(caller, "left_return") || is(caller, "right_return"))
	{
		award(10000);
		(is(caller, "left_return") ? ms.RecaptureLeft : ms.RecaptureRight) = true;
		light("recapture_activation_light01", ms.RecaptureLeft);
		light("recapture_activation_light02", ms.RecaptureRight);
		if (ms.RecaptureLeft && ms.RecaptureRight && !ms.RecaptureArmed)
		{
			ms.RecaptureArmed = true;
			light("recapture_ball_light", true);
		}
		return true;
	}
	if (is(caller, "left_outlane") || is(caller, "right_outlane"))
	{
		award(25000);
		if (ms.RecaptureArmed)
		{
			ms.RecaptureArmed = ms.RecaptureLeft = ms.RecaptureRight = false;
			ms.RecaptureSave = true;
			light("recapture_ball_light", false);
			light("recapture_activation_light01", false);
			light("recapture_activation_light02", false);
			flash("ball_recapture_activated_light01");
			flash("ball_recapture_activated_light02");
		}
		return true;
	}

	// spinners (0x42ff88): 2,000 and the jackpot +10,000
	if (is(caller, "spinner01") || is(caller, "spinner02") || is(caller, "spinner03"))
	{
		award(2000);
		add_jackpot(10000);
		return true;
	}

	return false; // bumpers, slingshots and the rest: the generic rules
}

std::vector<std::string> ft2_mad::Status()
{
	std::vector<std::string> lines;
	if (!Active() || pb::game_mode != GameModes::InGame)
		return lines;
	auto& p = player();
	if (ms.Mode)
	{
		auto line = std::string(ExperimentNames[ms.Mode]);
		if (ms.Mode != BringToLife)
			line += "  " + std::to_string(static_cast<int>(ms.ModeLeft + 0.5f)) + " s";
		lines.push_back(line);
		if (ms.Mode == AngryMob)
			lines.push_back("Mob: " + std::to_string(ms.MobHits) + " of 9");
		if (ms.Mode == BringToLife)
			lines.push_back(ms.Wizard ? "Tower sink: bring it to life" : "Hit the seven monster targets");
	}
	else if (ms.NextMode)
		lines.push_back(std::string("Igor: ") + ExperimentNames[ms.NextMode]);
	lines.push_back(flask_contents());
	int parts = 0;
	for (auto got : p.Parts)
		parts += got;
	lines.push_back("Monster parts " + std::to_string(parts) + "/6" +
		(p.BrainIndex >= 0 ? std::string("  Brain: ") + BrainNames[p.BrainIndex] : std::string()));
	lines.push_back("Jackpot " + commas(p.Jackpot) + (ms.Multiplier > 1 ? "  " + std::to_string(ms.Multiplier) + "X" : ""));
	if (ms.TeleporterOn)
		lines.push_back("Teleporter lit");
	if (ms.CloningLit)
		lines.push_back("Clone ball lit (mercury)");
	if (ms.PlasmaStep)
		lines.push_back("Plasma field: shot " + std::to_string(ms.PlasmaStep) + " of 3");
	if (ms.ExtraBallLit)
		lines.push_back("Extra ball lit (element A)");
	return lines;
}
