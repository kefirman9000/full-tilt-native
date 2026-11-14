#include "pch.h"
#include "ft2_alien.h"

#include "ft2_animation.h"
#include "loader.h"
#include "pb.h"
#include "table_art.h"
#include "timer.h"
#include "TBall.h"
#include "TBlocker.h"
#include "TKickout.h"
#include "TLight.h"
#include "TLightGroup.h"
#include "TPinballTable.h"
#include "TPlunger.h"
#include "TPopupTarget.h"
#include "TSink.h"
#include "TSound.h"
#include "TTextBox.h"

// Alien Daze rules, ported from Alien.exe (alien\src\control.cpp). Addresses are the original functions, so each
// rule can be checked against the disassembly (ft2/alien_rules.md has the notes).
namespace
{
	enum Mission { CropCircles, Abduct, Bigfoot, JoyRide, Reassemble, MissionCount };
	const int MissionText[MissionCount] = {3276, 3277, 3278, 3279, 3280};    // "Mission:\n Make Crop Circles"...
	const int NextMissionText[MissionCount] = {3266, 3267, 3268, 3269, 3270}; // "Next Mission: ..."
	const int FinishedText[MissionCount] = {3271, 3272, 3273, 3274, 3275};    // "Crop Circle Finished"...
	const char* MissionNames[MissionCount] = {"Make Crop Circles", "Abduct Humans", "Re-capture Bigfoot",
	                                          "Take a Joy Ride", "Re-assemble Abductee"};
	const float MissionSeconds[MissionCount] = {30, 25, 35, 25, 0}; // 0x443540 (the last one has no time limit)

	// 0x4439c4: the 17 abduction targets and their lights
	const char* AbductTargets[17][2] = {
		{"a_spot04", "v_lite74"}, {"a_spot03", "v_lite73"}, {"a_spot02", "v_lite75"}, {"a_spot01", "v_lite76"},
		{"a_spot05", "v_lite97"}, {"a_spot06", "v_lite98"}, {"a_drop07", "v_lite94"}, {"a_drop06", "v_lite95"},
		{"a_drop05", "v_lite96"}, {"a_drop01", "recharge_lower_light"}, {"a_drop02", "recharge_upper_light"},
		{"a_drop09", "v_lite55"}, {"a_drop10", "v_lite54"}, {"a_drop11", "v_lite53"}, {"a_drop12", "v_lite52"},
		{"a_drop13", "v_lite51"}, {"a_drop14", "v_lite50"}};
	const int AbducteeTextBase[5] = {3132, 3149, 3166, 3183, 3200}; // 0x443a90: a new crowd every round

	// 0x443ae0: Re-assemble Abductee. The six UFO targets are the right parts, the three space alien targets the
	// wrong ones, each taking the place of one right part (0x42e3b8)
	const char* BodyTargets[9] = {"a_drop09", "a_drop10", "a_drop11", "a_drop12", "a_drop13", "a_drop14",
	                              "a_drop07", "a_drop06", "a_drop05"};
	const char* BodyLights[6] = {"v_lite55", "v_lite54", "v_lite53", "v_lite52", "v_lite51", "v_lite50"};
	const int WrongPartSlot[3] = {2, 0, 5}; // horse head for the head, chicken legs for the legs, cow brain

	const char* GearTargets[4] = {"a_spot04", "a_spot03", "a_spot02", "a_spot01"};
	const char* GearLights[4] = {"v_lite74", "v_lite73", "v_lite75", "v_lite76"};
	const char* Sockets[4] = {"socket01", "socket02", "socket03", "socket04"};
	const char* StunAim[8] = {"v_lite85", "v_lite86", "v_lite87", "v_lite88", "v_lite89", "v_lite90", "v_lite91",
	                          "v_lite92"};
	// 0x443c80: Bigfoot walks from spot i to spot i+1 along these footprints
	const int PathLength[6] = {14, 6, 8, 4, 3, 1};

	const int BumperScores[5] = {1500, 3000, 4500, 6000, 7500};       // 0x440f00
	const int UfoScores[5] = {5000, 10000, 15000, 20000, 30000};      // 0x441318
	const int LargeCrop[5] = {100000, 200000, 300000, 400000, 500000}; // 0x441b18
	const int MediumCrop[5] = {200000, 400000, 800000, 800000, 800000};
	const int SmallCrop[5] = {50000, 75000, 100000, 125000, 150000};
	const int LandingScores[5] = {25000, 50000, 100000, 250000, 500000}; // s_ramp15, by landing gear down
	const int SpiralScores[4] = {50000, 100000, 200000, 400000};          // s_ramp01
	const int LoopScores[4] = {40000, 80000, 160000, 320000};             // s_ramp13

	struct PlayerState // what the original keeps per player (0x43257c / 0x432658)
	{
		bool MissionDone[MissionCount]{};
		bool Gear[4]{};
		int Locks = 0;
	};

	struct AlienState
	{
		TPinballTable* Table = nullptr;
		std::map<std::string, TPinballComponent*> Parts;
		PlayerState Players[4];

		int Multiplier = 1; // 0x448a80: 1-3, raised by the UFO targets
		int UfoStage = 0;   // 0x448b94
		long long Jackpot = 1000000;
		int BumperStage = 0, BumperHits = 0;
		bool BeamOn = false;

		int NextMission = -1;   // 0x44353c
		bool MissionOn = false; // 0x448a70
		float MissionLeft = 0;
		int MissionTimer = 0;
		int AbductRound = 0, Abducted = 0;
		bool AbductHit[17]{};
		int BodySlot[6]{}; // 0 empty, 1 right part, 2 wrong part
		bool Assembled = false;
		int TagIndex = 0, BuzzIndex = 0, TabloidIndex = 0;
		int CropLarge = 0, CropMedium = 0, CropSmall = 0;

		int BigfootAt = 0, BigfootStep = 0, BigfootTimer = 0;
		bool BigfootUp = false;

		bool Landed = false; // 0x448ae4
		int LandingStep = 0, LandingTimer = 0;
		int ReserveCharges = 0;
		int SpiralLit = 0, SpiralTimer = 0, LoopLit = 0, LoopTimer = 0;
		bool BonusHold = false;
		long long HeldBonus = 0;
		bool ExtraBallLit = false, ExtraBallGiven = false;
		bool CornLeft = false, CornRight = false;
		bool StunReady = false, StunHolding = false;
		int StunTimer = 0, StunAimIndex = 0, StunAimTimer = 0;
		int SpaceSinkShots = 0;
		int Choice = 0; // 1 artifact trade, 2 extend or cancel the mission
		int ChoiceTimer = 0, TradeItem = 0;
		TSink* ChoiceSink = nullptr;
		int ReleaseTimer = 0, ToRelease = 0;

		// this ball's bonus (0x448aac..0x448ac0)
		int MissionsThisBall = 0, Abductees = 0, Artifacts = 0, Encasements = 0;
		bool LandedThisBall = false;
	} as;

	TPinballTable* table()
	{
		return pb::MainTable;
	}

	PlayerState& player()
	{
		auto index = table() ? table()->CurrentPlayer : 0;
		return as.Players[std::max(0, std::min(3, index))];
	}

	TPinballComponent* part(const char* name)
	{
		auto found = as.Parts.find(name);
		if (found != as.Parts.end())
			return found->second;
		TPinballComponent* component = nullptr;
		for (auto candidate : table()->ComponentList)
			if (candidate->GroupName && strcmp(candidate->GroupName, name) == 0)
			{
				component = candidate;
				break;
			}
		as.Parts[name] = component;
		if (!component && pb::DevDebug)
			printf("FT2DBG alien: no part named %s\n", name);
		return component;
	}

	bool is(TPinballComponent* caller, const char* name)
	{
		return caller && caller->GroupName && strcmp(caller->GroupName, name) == 0;
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

	// A text of Alien.exe, cleaned for display: '>' is an apostrophe, '`' and '[' are padding and icons, '@' an X
	std::string text(int id, const std::string& fallback)
	{
		auto raw = table_art::Text(id, fallback);
		std::string out;
		for (size_t i = 0; i < raw.size(); i++)
		{
			auto c = raw[i];
			if (c == '>')
				out += '\'';
			else if (c == '@')
				out += 'X';
			else if (c == '\n')
				out += ' ';
			else if (c != '`' && c != '[')
				out += c;
			if (out.size() >= 2 && out.back() == ' ' && out[out.size() - 2] == ' ')
				out.pop_back(); // the texts break lines with "\n " (one space is enough on one line)
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
			printf("FT2DBG alien: %s\n", message.c_str());
		if (pb::InfoTextBox)
			pb::InfoTextBox->Display(message.c_str(), seconds);
	}

	void show_mission(const std::string& message)
	{
		if (pb::DevDebug)
			printf("FT2DBG alien mission: %s\n", message.c_str());
		if (pb::MissTextBox && pb::MissTextBox != pb::InfoTextBox)
			pb::MissTextBox->Display(message.c_str(), -1.0f);
		else
			show(message, 4.0f);
	}

	// 0x42c768: every score is multiplied by the multiplier
	void award(long long points)
	{
		if (table() && !table()->TiltLockFlag && points > 0)
			table()->AddScore(static_cast<int>(points * as.Multiplier));
	}

	// 0x42c7c8: the jackpot grows with multiplied points too, up to 10,000,000
	void jackpot(long long points)
	{
		as.Jackpot = std::min<long long>(10000000, as.Jackpot + points * as.Multiplier);
	}

	bool single_ball()
	{
		return table()->MultiballCount <= 1;
	}

	void reset_targets(std::initializer_list<const char*> names)
	{
		for (auto name : names)
			if (auto target = dynamic_cast<TPopupTarget*>(part(name)))
				if (!target->ActiveFlag)
					target->Message(MessageCode::TPopupTargetEnable, 0.0f);
	}

	bool down(const char* name)
	{
		auto target = dynamic_cast<TPopupTarget*>(part(name));
		return target && !target->ActiveFlag;
	}

	void eject(TPinballComponent* pocket, float seconds = 1.0f)
	{
		if (dynamic_cast<TKickout*>(pocket))
			pocket->Message(MessageCode::TKickoutRestartTimer, seconds);
		else if (dynamic_cast<TSink*>(pocket))
			pocket->Message(MessageCode::TSinkResetTimer, seconds);
	}

	// A ball taken out of play (locked in the spacecraft, lifted by the tractor beam): a new one comes to the plunger
	void ball_taken()
	{
		auto t = table();
		t->MultiballCount = std::max(0, t->MultiballCount - 1);
		if (t->MultiballCount == 0 && t->Plunger)
			t->Plunger->Message(MessageCode::PlungerStartFeedTimer, 0.0f);
	}

	// ---- Missions (0x42cd20 choose, 0x42d16c start, 0x42ceb0 stop, 0x42cfb4 completed) ----
	void choose_next_mission()
	{
		auto& done = player().MissionDone;
		if (done[0] && done[1] && done[2] && done[3] && done[4])
			for (auto& d : done)
				d = false;
		int choice;
		if (done[0] && done[1] && done[2] && done[3])
			choice = Reassemble; // the last mission, once the other four are done
		else
		{
			// a random mission not done yet; a different one from the last pick when there is a choice
			std::vector<int> open;
			for (int i = 0; i < 4; i++)
				if (!done[i])
					open.push_back(i);
			if (open.size() > 1)
				open.erase(std::remove(open.begin(), open.end(), as.NextMission), open.end());
			choice = open[rand() % open.size()];
		}
		static const char* modeLights[MissionCount] = {"mode_lite1", "mode_lite2", "mode_lite3", "mode_lite4",
		                                               "mode_lite5"};
		for (int i = 0; i < MissionCount; i++)
			light(modeLights[i], done[i]);
		as.NextMission = choice;
		flash(modeLights[choice]);
		flash("v_lite66");
		show_mission(text(NextMissionText[choice], std::string("Next Mission: ") + MissionNames[choice]));
	}

	void stop_bigfoot();
	void reset_abduction();
	void reset_body();

	void stop_mission_parts(int mission)
	{
		switch (mission)
		{
		case Abduct:
			for (auto& target : AbductTargets)
				light(target[1], false);
			break;
		case Bigfoot:
			stop_bigfoot();
			break;
		case JoyRide:
			light("flames", false);
			send("port_ship_lights", MessageCode::TLightGroupResetAndTurnOff);
			send("starboard_ship_lights", MessageCode::TLightGroupResetAndTurnOff);
			send("loop_lights", MessageCode::TLightGroupResetAndTurnOff);
			for (auto name : {"v_lite94", "v_lite95", "v_lite96"})
				light(name, false);
			break;
		case Reassemble:
			for (auto name : BodyLights)
				light(name, false);
			send("loop_lights", MessageCode::TLightGroupResetAndTurnOff);
			send("landing_lights", MessageCode::TLightGroupResetAndTurnOff);
			break;
		default:
			break;
		}
		light("v_lite66", false);
	}

	void mission_over(bool completed)
	{
		auto mission = as.NextMission;
		kill(as.MissionTimer);
		if (!as.MissionOn || mission < 0)
			return;
		as.MissionOn = false;
		stop_mission_parts(mission);
		if (completed)
		{
			player().MissionDone[mission] = true;
			as.MissionsThisBall++;
			show_mission(text(FinishedText[mission], std::string(MissionNames[mission]) + " Finished"));
		}
		else
			show_mission(std::string(MissionNames[mission]) + " cancelled");
		choose_next_mission();
	}

	void mission_tick(int, void*)
	{
		as.MissionTimer = 0;
		if (!as.MissionOn)
			return;
		as.MissionLeft -= 1.0f;
		if (as.MissionLeft <= 0.0f)
		{
			mission_over(true); // a timed mission is done when its time is up (0x42d154)
			return;
		}
		as.MissionTimer = timer::set(1.0f, nullptr, mission_tick);
	}

	void start_bigfoot();

	void start_mission()
	{
		if (as.MissionOn || as.NextMission < 0)
			return;
		auto mission = as.NextMission;
		as.MissionOn = true;
		as.MissionLeft = MissionSeconds[mission];
		kill(as.MissionTimer);
		if (as.MissionLeft > 0)
			as.MissionTimer = timer::set(1.0f, nullptr, mission_tick);
		wave("wave46");
		show_mission(text(MissionText[mission], std::string("Mission: ") + MissionNames[mission]));
		flash("v_lite66");
		switch (mission)
		{
		case CropCircles: // 0x42d16c case 0
			as.CropLarge = as.CropMedium = as.CropSmall = 0;
			break;
		case Abduct: // 0x42dff4
			wave("wave50");
			reset_abduction();
			break;
		case Bigfoot: // 0x42f62c
			wave("wave61");
			start_bigfoot();
			break;
		case JoyRide: // 0x42d560
			wave("wave64");
			light("flames", true);
			send("port_ship_lights", MessageCode::TLightGroupOffsetAnimationForward, 0.05f);
			send("starboard_ship_lights", MessageCode::TLightGroupOffsetAnimationForward, 0.05f);
			send("loop_lights", MessageCode::TLightGroupOffsetAnimationBackward, 0.5f);
			for (auto name : {"v_lite94", "v_lite95", "v_lite96"})
				flash(name);
			reset_targets({"a_drop05", "a_drop06", "a_drop07"});
			break;
		case Reassemble: // 0x42e31c
			wave("wave77");
			reset_body();
			break;
		default:
			break;
		}
	}

	// ---- Abduct Humans (0x42df88, 0x42e088) ----
	void reset_abduction()
	{
		for (int i = 0; i < 17; i++)
		{
			as.AbductHit[i] = false;
			flash(AbductTargets[i][1]);
		}
		reset_targets({"a_drop05", "a_drop06", "a_drop07", "a_drop01", "a_drop02", "a_drop09", "a_drop10",
		               "a_drop11", "a_drop12", "a_drop13", "a_drop14"});
		as.Abducted = 0;
	}

	bool abduct_hit(TPinballComponent* caller)
	{
		for (int i = 0; i < 17; i++)
			if (is(caller, AbductTargets[i][0]))
			{
				if (!as.AbductHit[i])
				{
					as.AbductHit[i] = true;
					light(AbductTargets[i][1], true);
					show(text(AbducteeTextBase[as.AbductRound] + i, "Abducted!") + " abducted", 3.0f);
					char name[8];
					snprintf(name, sizeof name, "wave%02d", 51 + rand() % 10);
					wave(name);
					award(std::min(20000 + 2000 * as.Abducted, 100000));
					as.Abducted++;
					as.Abductees++;
				}
				bool all = true;
				for (auto hit : as.AbductHit)
					all = all && hit;
				if (all) // everyone abducted: a new crowd (0x42e01c)
				{
					as.AbductRound = (as.AbductRound + 1) % 5;
					wave("wave50");
					reset_abduction();
				}
				return true;
			}
		return false;
	}

	// ---- Re-assemble Abductee (0x42e238, 0x42e3b8, 0x42e510) ----
	void reset_body()
	{
		for (auto& slot : as.BodySlot)
			slot = 0;
		as.Assembled = false;
		reset_targets({"a_drop09", "a_drop10", "a_drop11", "a_drop12", "a_drop13", "a_drop14", "a_drop05",
		               "a_drop06", "a_drop07"});
		for (auto name : BodyLights)
			flash(name);
	}

	void body_part(TPinballComponent* caller)
	{
		for (int i = 0; i < 9; i++)
		{
			if (!is(caller, BodyTargets[i]))
				continue;
			show(text(3246 + i, "Body part"), 3.0f);
			if (i < 6)
			{
				light(BodyLights[i], true);
				if (as.BodySlot[i] != 2)
				{
					as.BodySlot[i] = 1;
					wave("wave78");
					award(100000);
				}
			}
			else
			{
				wave("wave80");
				as.BodySlot[WrongPartSlot[i - 6]] = 2; // a wrong part takes that place
				light(BodyLights[WrongPartSlot[i - 6]], true);
				award(50000);
			}
		}
		bool complete = true, wrong = false;
		for (auto slot : as.BodySlot)
		{
			complete = complete && slot != 0;
			wrong = wrong || slot == 2;
		}
		if (complete && !as.Assembled)
		{
			as.Assembled = true;
			wave(wrong ? "wave86" : "wave85");
			award(wrong ? 200000 : 400000);
			show(text(3281, "Release Abductee") + " (landing ramp) or " + text(3282, "Keep Abductee") +
			     " (loop)", 6.0f);
			send("loop_lights", MessageCode::TLightGroupOffsetAnimationForward, 0.1f);
			send("landing_lights", MessageCode::TLightGroupOffsetAnimationForward, 0.1f);
		}
	}

	void abductee_decided(bool release)
	{
		show(release ? text(3281, "Release Abductee") : text(3256, "Eat for Lunch"), 4.0f);
		wave(release ? "wave83" : "wave82");
		award(as.Jackpot + 500000); // jackpot + 500,000, then the jackpot starts again (0x42e510)
		as.Jackpot = 1000000;
		if (!release)
			as.Encasements++;
		mission_over(true);
	}

	// ---- Re-capture Bigfoot (0x42f62c, 0x42f498, 0x42f58c, 0x42f74c) ----
	std::string bigfoot_name(int index)
	{
		char name[16];
		snprintf(name, sizeof name, "a_bigfoot%02d", index + 1);
		return name;
	}

	std::string footprint(int path, int step)
	{
		char name[24];
		snprintf(name, sizeof name, "Path%dPrint%02d", path == 0 ? 6 : path, step + 1);
		return name;
	}

	void clear_footprints()
	{
		for (int path = 0; path < 6; path++)
			for (int step = 0; step < PathLength[path]; step++)
				light(footprint(path, step).c_str(), false);
	}

	void bigfoot_down(int index)
	{
		send(bigfoot_name(index).c_str(), MessageCode::TPopupTargetDisable);
	}

	void bigfoot_walk(int, void*);

	void bigfoot_appear(int index)
	{
		as.BigfootAt = index;
		as.BigfootUp = true;
		as.BigfootStep = 0;
		clear_footprints();
		send(bigfoot_name(index).c_str(), MessageCode::TPopupTargetEnable);
		show(text(3217, "Bigfoot"), 1.0f);
		wave("wave63");
		kill(as.BigfootTimer);
		as.BigfootTimer = timer::set(5.0f, nullptr, bigfoot_walk); // 5 seconds, then he moves on
	}

	// 0x42f58c / 0x42f498: he leaves, and footprints light one by one toward the next spot
	void bigfoot_walk(int, void*)
	{
		as.BigfootTimer = 0;
		if (!as.MissionOn || as.NextMission != Bigfoot)
			return;
		auto next = (as.BigfootAt + 1) % 6;
		if (as.BigfootUp)
		{
			bigfoot_down(as.BigfootAt);
			as.BigfootUp = false;
			as.BigfootStep = 0;
		}
		auto path = next; // the path to spot n is the one stored for n (spot 1 is reached by path 6)
		if (as.BigfootStep < PathLength[path])
		{
			light(footprint(path, as.BigfootStep).c_str(), true);
			as.BigfootStep++;
			as.BigfootTimer = timer::set(1.5f / static_cast<float>(PathLength[path] + 1), nullptr, bigfoot_walk);
			return;
		}
		bigfoot_appear(next);
	}

	void start_bigfoot()
	{
		for (int i = 0; i < 6; i++)
			bigfoot_down(i);
		bigfoot_appear(rand() % 6);
	}

	void stop_bigfoot()
	{
		kill(as.BigfootTimer);
		for (int i = 0; i < 6; i++)
			bigfoot_down(i);
		as.BigfootUp = false;
		clear_footprints();
	}

	// ---- Spacecraft: landing gear, landing, lock and multiball ----
	void update_gear_lights()
	{
		auto& gear = player().Gear;
		bool all = true;
		for (int i = 0; i < 4; i++)
		{
			light(GearLights[i], gear[i]);
			all = all && gear[i];
		}
		light("v_lite83", all && as.Landed); // "Spacecraft Lock Enabled"
	}

	void update_sockets()
	{
		for (int i = 0; i < 4; i++)
			if (auto socket = dynamic_cast<TFT2Animation*>(part(Sockets[i])))
			{
				if (i < player().Locks)
					socket->Start();
				else
					socket->Stop();
			}
	}

	// 0x42f96c: gear targets a_spot01-04
	void gear_hit(TPinballComponent* caller)
	{
		auto& gear = player().Gear;
		for (int i = 0; i < 4; i++)
			if (is(caller, GearTargets[i]) && !gear[i])
			{
				gear[i] = true;
				if (!as.Landed)
					show(text(3002, "Gear Lowered"), 2.0f);
			}
		update_gear_lights();
		bool all = gear[0] && gear[1] && gear[2] && gear[3];
		if (all && as.Landed)
		{
			wave("wave08");
			show(text(3003, "Spacecraft Lock Enabled"), 3.0f);
			award(50000);
		}
		else
			award(as.Landed ? 15000 : 7500);
	}

	void landing_step_back(int, void*)
	{
		as.LandingTimer = 0;
		if (as.LandingStep > 0)
			as.LandingStep--;
		light("v_lite72", as.LandingStep >= 1);
		light("v_lite71", as.LandingStep >= 2);
		light("v_lite70", as.LandingStep >= 3);
		if (as.LandingStep > 0)
			as.LandingTimer = timer::set(30.0f, nullptr, landing_step_back);
	}

	// 0x43120c landing ramp
	void landing_ramp(TPinballComponent* caller)
	{
		if (!as.MissionOn)
			choose_next_mission(); // the landing ramp picks another mission
		else if (as.NextMission == JoyRide)
		{
			wave("wave31");
			award(25000);
			return;
		}
		if (!as.Landed) // 0x430de4: land the spacecraft, scored by the landing gear that is down
		{
			int gear = 0;
			for (auto down : player().Gear)
				gear += down ? 1 : 0;
			show(text(3113 + gear, "Landed"), 4.0f);
			char name[8];
			snprintf(name, sizeof name, "wave%02d", 33 + gear);
			wave(name);
			award(LandingScores[gear]);
			as.Landed = true;
			as.LandedThisBall = true;
			update_gear_lights();
			return;
		}
		// 0x431088: after landing, each pass climbs the landing lights: 75,000 / 150,000 / 300,000 / 600,000
		static const int steps[4] = {75000, 150000, 300000, 600000};
		wave(as.LandingStep >= 3 ? "wave32" : "wave31");
		award(steps[std::min(as.LandingStep, 3)]);
		if (as.LandingStep < 3)
			as.LandingStep++;
		light("v_lite72", as.LandingStep >= 1);
		light("v_lite71", as.LandingStep >= 2);
		light("v_lite70", as.LandingStep >= 3);
		kill(as.LandingTimer);
		as.LandingTimer = timer::set(30.0f, nullptr, landing_step_back);
	}

	void release_next(int, void*)
	{
		as.ReleaseTimer = 0;
		auto exit = dynamic_cast<TSink*>(part("ship_exit_sink"));
		auto t = table();
		if (!exit || as.ToRelease <= 0)
			return;
		if (auto ball = t->AddBall(exit->BallPosition))
		{
			ball->throw_ball(&exit->BallThrowDirection, exit->ThrowAngleMult, exit->ThrowSpeedMult1,
			                 exit->ThrowSpeedMult2);
			t->MultiballCount++;
		}
		as.ToRelease--;
		if (as.ToRelease > 0)
			as.ReleaseTimer = timer::set(1.0f, nullptr, release_next);
	}

	// 0x4318cc: s_sink01 with the lock lit keeps the ball in the spacecraft; the fourth starts multiball
	bool spacecraft_lock(TSink* sink)
	{
		auto& p = player();
		award(100000);
		p.Locks++;
		update_sockets();
		if (p.Locks >= 4)
		{
			show(text(3006, "Multiball"), 4.0f);
			wave("wave19");
			p.Locks = 0;
			for (auto& gear : p.Gear)
				gear = false;
			as.Landed = false;
			update_gear_lights();
			update_sockets();
			as.ToRelease = 3; // the three waiting balls come out; this one goes back in play too
			kill(as.ReleaseTimer);
			as.ReleaseTimer = timer::set(1.0f, nullptr, release_next);
			eject(sink, 0.5f);
			return true;
		}
		show(text(3005, "Lock Ball"), 3.0f);
		wave("wave19");
		ball_taken(); // this one stays in the spacecraft; a new ball comes to the plunger
		return true;
	}

	// ---- Tractor beam (0x42dd74 bumpers, 0x42dcac kickout, 0x42da44 levitation) ----
	void set_beam(bool on)
	{
		as.BeamOn = on;
		if (auto kickout = dynamic_cast<TKickout*>(part("s_kout01")))
		{
			kickout->NotSomeFlag = on ? 0 : 1;
			if (!kickout->BallCaputeredFlag)
				kickout->ActiveFlag = on ? 1 : 0;
		}
		light("a_cover", !on);
		light("v_lite80", on);
	}

	void beam_lift(int, void* data)
	{
		auto kickout = static_cast<TKickout*>(data);
		if (!kickout || !kickout->BallCaputeredFlag || !kickout->Ball)
			return;
		// the ball floats up into the spacecraft (encasement) and a new one comes to the plunger
		kickout->Ball->Disable();
		kickout->Ball = nullptr;
		kickout->BallCaputeredFlag = 0;
		as.Encasements++;
		award(as.Encasements >= 7 ? 1000000 : 100000 + 100000LL * (as.Encasements - 1));
		if (as.Encasements >= 7)
			jackpot(1000000);
		show(text(3118, "Tractor Beam Ball Lock"), 3.0f);
		set_beam(false);
		ball_taken();
	}

	// ---- Stun magnet (0x42fea0, 0x42ff6c) ----
	void stun_aim_tick(int, void*)
	{
		as.StunAimTimer = 0;
		if (!as.StunHolding)
			return;
		light(StunAim[as.StunAimIndex], false);
		as.StunAimIndex = (as.StunAimIndex + 1) % 8;
		light(StunAim[as.StunAimIndex], true);
		as.StunAimTimer = timer::set(0.25f, nullptr, stun_aim_tick);
	}

	void stun_fire(int = 0, void* = nullptr)
	{
		as.StunTimer = 0;
		auto kickout = dynamic_cast<TKickout*>(part("stun_kickout"));
		if (!as.StunHolding || !kickout)
			return;
		as.StunHolding = false;
		kill(as.StunAimTimer);
		// aim at the lit light (its position is attribute 501)
		if (auto aim = part(StunAim[as.StunAimIndex]))
			if (auto pos = loader::query_float_attribute(aim->GroupIndex, 0, 501))
			{
				vector3 dir{pos[0] - kickout->Circle.Center.X, pos[1] - kickout->Circle.Center.Y, 0.0f};
				auto len = std::sqrt(dir.X * dir.X + dir.Y * dir.Y);
				if (len > 0.001f)
					kickout->BallThrowDirection = {dir.X / len, dir.Y / len, 0.0f};
			}
		wave("wave44");
		kickout->Message(MessageCode::TKickoutRestartTimer, 0.05f);
		for (auto name : StunAim)
			light(name, false);
		send("stun_lights", MessageCode::TLightGroupResetAndTurnOff);
		light("v_lite84", false);
		light("v_lite85", false);
		kickout->NotSomeFlag = 1; // back to rolling over it
	}

	void stun_grab(TBall* ball)
	{
		auto kickout = dynamic_cast<TKickout*>(part("stun_kickout"));
		if (!kickout || kickout->BallCaputeredFlag)
			return;
		as.StunReady = false;
		as.StunHolding = true;
		kickout->ActiveFlag = 1;
		kickout->Collision(ball, nullptr, nullptr, 0.0f, nullptr);
		kickout->Message(MessageCode::TKickoutRestartTimer, 60.0f); // held until fired
		wave("wave43");
		award(250000);
		show(text(3119, "Stun Magnet Active") + "  (flip to fire)", 4.0f);
		send("stun_lights", MessageCode::TLightGroupOffsetAnimationForward, 0.1f);
		as.StunAimIndex = 0;
		light(StunAim[0], true);
		kill(as.StunAimTimer);
		as.StunAimTimer = timer::set(0.25f, nullptr, stun_aim_tick);
		kill(as.StunTimer);
		as.StunTimer = timer::set(15.0f, nullptr, stun_fire);
	}

	// ---- Bonus hold (s_ramp01, 0x430c48) and loop (s_ramp13, 0x43147c) ----
	void spiral_step_back(int, void*)
	{
		as.SpiralTimer = 0;
		if (as.SpiralLit > 0)
			as.SpiralLit--;
		light("v_lite69", as.SpiralLit >= 1);
		light("v_lite68", as.SpiralLit >= 2);
		light("v_lite67", as.SpiralLit >= 3);
		if (as.SpiralLit > 1)
			as.SpiralTimer = timer::set(25.0f, nullptr, spiral_step_back);
	}

	void spiral_ramp()
	{
		award(SpiralScores[std::min(as.SpiralLit, 3)]);
		if (as.SpiralLit < 3)
			as.SpiralLit++;
		wave(as.SpiralLit == 3 ? "wave30" : "wave29");
		if (as.SpiralLit == 2)
			show(text(3245, "Once More to Hold Bonus"), 3.0f);
		else if (as.SpiralLit == 3 && !as.BonusHold)
		{
			as.BonusHold = true;
			show(text(3111, "Bonus Held"), 3.0f);
		}
		light("v_lite69", as.SpiralLit >= 1);
		light("v_lite68", as.SpiralLit >= 2);
		light("v_lite67", as.SpiralLit >= 3);
		kill(as.SpiralTimer);
		as.SpiralTimer = timer::set(25.0f, nullptr, spiral_step_back);
	}

	void light_extra_ball()
	{
		if (as.ExtraBallGiven || as.ExtraBallLit)
			return;
		as.ExtraBallLit = true;
		flash("v_lite65");
		show("Extra ball lit at the cornfield", 3.0f);
	}

	void loop_step_back(int, void*)
	{
		as.LoopTimer = 0;
		if (as.LoopLit > 0)
			as.LoopLit--;
		light("v_lite79", as.LoopLit >= 1);
		light("v_lite78", as.LoopLit >= 2);
		light("v_lite77", as.LoopLit >= 3);
		if (as.LoopLit > 1)
			as.LoopTimer = timer::set(20.0f, nullptr, loop_step_back);
	}

	void loop_ramp()
	{
		if (as.MissionOn && as.NextMission == Reassemble && as.Assembled)
		{
			abductee_decided(false);
			return;
		}
		if (as.MissionOn && as.NextMission == JoyRide) // 0x431570: tabloid appearances
		{
			wave("wave65");
			show(text(3236 + as.TabloidIndex, "Tabloid photo"), 3.0f);
			as.TabloidIndex = (as.TabloidIndex + 1) % 9;
			award(50000);
			return;
		}
		award(LoopScores[std::min(as.LoopLit, 3)]);
		if (as.LoopLit < 3)
			as.LoopLit++;
		wave(as.LoopLit == 3 ? "wave39" : "wave38");
		if (as.LoopLit == 3)
			light_extra_ball();
		light("v_lite79", as.LoopLit >= 1);
		light("v_lite78", as.LoopLit >= 2);
		light("v_lite77", as.LoopLit >= 3);
		kill(as.LoopTimer);
		as.LoopTimer = timer::set(20.0f, nullptr, loop_step_back);
	}

	// ---- Reserve power (a_drop01/02, 0x42eb8c; outlane kickback power_reserve_*) ----
	void update_reserve()
	{
		light("v_lite64", as.ReserveCharges >= 1);
		light("v_lite63", as.ReserveCharges >= 2);
		light("v_lite62", as.ReserveCharges >= 3);
		bool on = as.ReserveCharges > 0;
		for (auto name : {"power_reserve_intake", "power_reserve_outlet"})
			if (auto pocket = part(name))
				pocket->ActiveFlag = on ? 1 : 0;
		light("power_reserve_door", on);
		light("power_reserve_light", on);
	}

	// ---- Choices on the flippers (0x431c24 trade / extend, 0x431e3c, 0x431f84) ----
	void choice_timeout(int, void*);

	void offer_choice(int kind, TSink* sink)
	{
		as.Choice = kind;
		as.ChoiceSink = sink;
		if (kind == 1)
		{
			as.TradeItem = rand() % 80;
			auto pattern = text(3007, "Trade Alien Artifact for %s? Yes (Right Flipper) or No (Left Flipper).");
			auto at = pattern.find("%s");
			if (at != std::string::npos)
				pattern.replace(at, 2, text(3008 + as.TradeItem, "something"));
			wave("wave23");
			show(pattern, 10.0f);
		}
		else
			show(text(3258, "Extend Mission (right flipper) or Cancel Mission (left flipper)"), 10.0f);
		kill(as.ChoiceTimer);
		as.ChoiceTimer = timer::set(10.0f, nullptr, choice_timeout);
	}

	void choose(bool right)
	{
		auto kind = as.Choice;
		as.Choice = 0;
		kill(as.ChoiceTimer);
		if (pb::InfoTextBox)
			pb::InfoTextBox->Clear();
		if (kind == 1)
		{
			// 0x431e3c: the trade is worth 50,000 + 1,000 per artifact number (10,000 + for the junk past 39);
			// "yes" pays it in full for the good ones, "no" pays half (and the other way round for junk)
			auto junk = as.TradeItem > 39;
			long long value = as.TradeItem * 1000 + (junk ? 10000 : 50000);
			if (right)
			{
				wave("wave25");
				as.Artifacts++;
				award(junk ? value / 2 : value);
			}
			else
			{
				wave("wave24");
				award(junk ? value : value / 2);
			}
		}
		else if (kind == 2)
		{
			if (right) // 0x431f84: 15 more seconds
			{
				as.MissionLeft += 15.0f;
				show("Mission extended", 2.0f);
			}
			else
				mission_over(false);
		}
		if (as.ChoiceSink)
			eject(as.ChoiceSink, 0.3f);
		as.ChoiceSink = nullptr;
	}

	void choice_timeout(int, void*)
	{
		as.ChoiceTimer = 0;
		if (as.Choice)
			choose(as.Choice == 2); // no answer: keep the mission going / decline the trade
	}

	// ---- Cornfield (corn_sink_1-3, 0x4309a4) ----
	bool cornfield(TPinballComponent* caller)
	{
		bool sink2 = is(caller, "corn_sink_2"), sink3 = is(caller, "corn_sink_3");
		if (as.MissionOn && as.NextMission == CropCircles) // 0x430510
		{
			long long points;
			std::string message;
			if (sink3)
			{
				points = SmallCrop[as.CropSmall];
				message = format(3131, "Small Crop Circle: %ld points", points * as.Multiplier);
				as.CropSmall = std::min(as.CropSmall + 1, 4);
				wave("wave47");
			}
			else if (sink2)
			{
				points = MediumCrop[as.CropMedium];
				message = format(3130, "Medium Crop Circle: %ld points", points * as.Multiplier);
				as.CropMedium = std::min(as.CropMedium + 1, 4);
				wave("wave48");
			}
			else
			{
				points = LargeCrop[as.CropLarge];
				message = format(3129, "Large Crop Circle: %ld points", points * as.Multiplier);
				as.CropLarge = std::min(as.CropLarge + 1, 4);
				wave("wave49");
			}
			show(message, 3.0f);
			award(points);
			return true;
		}
		if (as.MissionOn && as.NextMission == JoyRide && sink3) // 0x43079c: buzz the landmarks
		{
			char name[8];
			snprintf(name, sizeof name, "wave%02d", 65 + as.BuzzIndex % 4);
			wave(name);
			show(text(3218 + as.BuzzIndex, "Buzz"), 3.0f);
			as.BuzzIndex = (as.BuzzIndex + 1) % 9;
			award(50000);
			return true;
		}
		if (sink2 && as.ExtraBallLit) // 0x43095c
		{
			as.ExtraBallLit = false;
			as.ExtraBallGiven = true;
			light("v_lite65", false);
			table()->ExtraBalls++;
			show(text(3290, "Extra Ball"), 3.0f);
			wave("wave27");
			return true;
		}
		wave("wave17");
		if (is(caller, "corn_sink_1"))
			award(20000);
		else if (sink3)
			award(75000);
		else if (!as.MissionOn && single_ball())
			start_mission(); // 0x430868: the center hole starts the lit mission
		else
		{
			award(75000);
			jackpot(75000);
		}
		return true;
	}

	void reset_ball_state()
	{
		as.MissionsThisBall = as.Abductees = as.Artifacts = as.Encasements = 0;
		as.LandedThisBall = false;
	}
}

bool ft2_alien::Active()
{
	if (!table())
		return false;
	std::string key = pb::DatFileName;
	auto slash = key.find_last_of("/\\");
	if (slash != std::string::npos)
		key = key.substr(slash + 1);
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	if (key != "ALIEN.DAT")
		return false;
	if (as.Table != table())
	{
		as = AlienState{};
		as.Table = table();
	}
	return true;
}

void ft2_alien::NewGame()
{
	auto t = as.Table;
	as = AlienState{};
	as.Table = t;
	set_beam(false);
	update_reserve();
	update_gear_lights();
	update_sockets();
	stop_bigfoot();
	for (auto name : {"v_lite84", "v_lite85", "v_lite65", "v_lite69", "v_lite68", "v_lite67", "v_lite79",
	                  "v_lite78", "v_lite77", "v_lite72", "v_lite71", "v_lite70"})
		light(name, false);
	if (auto stun = dynamic_cast<TKickout*>(part("stun_kickout")))
	{
		stun->NotSomeFlag = 1; // the ball rolls over it until the magnet catches it
		if (!stun->BallCaputeredFlag)
			stun->ActiveFlag = 0;
	}
	choose_next_mission();
}

void ft2_alien::PlayerChanged()
{
	if (as.MissionOn)
		mission_over(false);
	as.Landed = false;
	as.LandingStep = 0;
	update_gear_lights();
	update_sockets();
	choose_next_mission();
}

void ft2_alien::Launch()
{
	set_beam(as.BeamOn);
}

// 0x432110: missions x 200,000, ship landed 100,000, abductees x 20,000, artifacts x 10,000, encasements x 100,000
void ft2_alien::EndOfBall()
{
	if (as.MissionOn)
		mission_over(true); // the drain ends a running mission, and it counts (0x4329e1 calls 0x42cfb4)
	kill(as.StunTimer);
	as.StunHolding = false;
	auto t = table();
	long long bonus = as.MissionsThisBall * 200000LL + (as.LandedThisBall ? 100000 : 0) + as.Abductees * 20000LL +
		as.Artifacts * 10000LL + as.Encasements * 100000LL;
	auto total = bonus + as.HeldBonus;
	if (total > 0 && !t->TiltLockFlag)
	{
		t->AddScore(static_cast<int>(total));
		std::string summary = format(3121, "Missions %ld", as.MissionsThisBall * 200000LL) + "  " +
			format(3122, "Ship Landed %ld", as.LandedThisBall ? 100000 : 0) + "  " +
			format(3123, "Abductees %ld", as.Abductees * 20000LL) + "  " +
			format(3124, "Artifacts Traded %ld", as.Artifacts * 10000LL) + "  " +
			format(3125, "Encasements %ld", as.Encasements * 100000LL) + "  " +
			(as.HeldBonus ? format(3283, "Held Bonus %ld", as.HeldBonus) + "  " : std::string()) +
			format(3291, "Total Bonus %ld", total);
		if (pb::InfoTextBox)
			pb::InfoTextBox->Display(summary.c_str(), 6.0f);
	}
	as.HeldBonus = as.BonusHold ? bonus : 0; // a held bonus counts again next ball
	as.BonusHold = false;
	as.SpiralLit = 0;
	kill(as.SpiralTimer);
	for (auto name : {"v_lite69", "v_lite68", "v_lite67"})
		light(name, false);
	as.Multiplier = 1;
	as.UfoStage = 0;
	set_beam(false);
	as.BumperHits = 0;
	reset_ball_state();
}

bool ft2_alien::Message(MessageCode code, TPinballComponent* caller)
{
	if (code == MessageCode::TFlipperExtend)
	{
		bool right = is(caller, "flpr02"), left = is(caller, "flpr01");
		if (!right && !left)
			return false;
		if (as.Choice)
		{
			choose(right);
			return true;
		}
		if (as.StunHolding) // 0x432030: a flip fires the stun kickout
		{
			kill(as.StunTimer);
			stun_fire();
			return true;
		}
	}
	return false;
}

void ft2_alien::Frame()
{
	if (!as.StunReady || !table())
		return;
	auto magnet = part("s_mag01");
	auto kickout = dynamic_cast<TKickout*>(part("stun_kickout"));
	if (!kickout || kickout->BallCaputeredFlag)
		return;
	// s_mag01: a circle of radius 13.35 around the stun kickout (attribute 600); while the stun is ready a ball on
	// the playfield that comes inside it is pulled in
	float radius = 13.35f;
	if (magnet)
		if (auto circle = loader::query_float_attribute(magnet->GroupIndex, 0, 600))
			if (circle[0] >= 1)
				radius = circle[3];
	for (auto ball : table()->BallList)
		if (ball->ActiveFlag && !ball->CollisionComp && (ball->CollisionMask & 1))
		{
			auto dx = ball->Position.X - kickout->Circle.Center.X, dy = ball->Position.Y - kickout->Circle.Center.Y;
			if (dx * dx + dy * dy < radius * radius * 0.25f)
			{
				stun_grab(ball);
				return;
			}
		}
}

bool ft2_alien::Collision(TPinballComponent* caller)
{
	auto t = table();
	if (!caller || t->TiltLockFlag)
		return false;
	if (pb::DevDebug)
		printf("FT2DBG alien hit %s\n", caller->GroupName ? caller->GroupName : "?");
	auto mission = as.MissionOn ? as.NextMission : -1;

	// bumpers a_bmpr01-04 (0x42dd74)
	if (is(caller, "a_bmpr01") || is(caller, "a_bmpr02") || is(caller, "a_bmpr03") || is(caller, "a_bmpr04"))
	{
		wave("wave01");
		award(BumperScores[as.BumperStage]);
		jackpot(2000);
		if (!as.MissionOn && !as.BeamOn && single_ball())
		{
			as.BumperHits++;
			if (as.BumperHits == 11)
				show(text(3001, "Bumpers Activate Tractor Beam"), 5.0f);
			else if (as.BumperHits >= 12)
			{
				as.BumperHits = 0;
				set_beam(true);
			}
		}
		return true;
	}
	if (is(caller, "a_rebo01") || is(caller, "a_rebo02")) // 0x42ded4
	{
		wave("wave01");
		award(350);
		return true;
	}
	if (is(caller, "a_spnr01")) // 0x42fd70: 1,000 and the jackpot grows 15,000
	{
		wave("wave45");
		award(1000);
		jackpot(15000);
		return true;
	}
	if (is(caller, "a_cow")) // 0x43007c
	{
		wave("wave93");
		award(25000);
		return true;
	}
	if (is(caller, "a_roll01") || is(caller, "a_roll05") || is(caller, "a_roll02") || is(caller, "a_roll03") ||
		is(caller, "a_roll04")) // 0x42f848
	{
		wave(is(caller, "a_roll01") || is(caller, "a_roll05") ? "wave40" : "wave41");
		award(is(caller, "a_roll05") ? 25000 : is(caller, "a_roll01") || is(caller, "a_roll02") ? 10000 : 5000);
		return true;
	}

	// drop and stand-up targets: each mission can claim them first
	bool target = dynamic_cast<TPopupTarget*>(caller) || strncmp(caller->GroupName ? caller->GroupName : "", "a_spot", 6) == 0;
	if (target && strncmp(caller->GroupName, "a_bigfoot", 9) == 0) // 0x42f74c
	{
		wave("wave62");
		if (mission == Bigfoot)
		{
			award(100000);
			show(text(3217, "Bigfoot") + " captured!", 2.0f);
			int next;
			do
				next = rand() % 6;
			while (next == as.BigfootAt);
			bigfoot_down(as.BigfootAt);
			bigfoot_appear(next);
		}
		return true;
	}
	if (target)
	{
		jackpot(5000);
		if (mission == Abduct && abduct_hit(caller))
			return true;
		if (mission == Reassemble)
		{
			for (auto name : BodyTargets)
				if (is(caller, name))
				{
					body_part(caller);
					return true;
				}
		}
	}

	// space alien targets a_drop05-07 (0x42e964): the bank lights the stun magnet (Play Tag in Joy Ride)
	if (is(caller, "a_drop05") || is(caller, "a_drop06") || is(caller, "a_drop07"))
	{
		wave("wave09");
		light(is(caller, "a_drop07") ? "v_lite94" : is(caller, "a_drop06") ? "v_lite95" : "v_lite96", true);
		if (down("a_drop05") && down("a_drop06") && down("a_drop07"))
		{
			if (mission == JoyRide) // 0x42e88c
			{
				wave("wave69");
				award(1000000);
				show(text(3227 + as.TagIndex, "Play Tag"), 4.0f);
				as.TagIndex = (as.TagIndex + 1) % 9;
			}
			else
			{
				award(10000);
				as.StunReady = true;
				light("v_lite84", true);
				show(text(3120, "Stun Ready"), 3.0f);
			}
			for (auto name : {"v_lite94", "v_lite95", "v_lite96"})
				light(name, false);
			reset_targets({"a_drop05", "a_drop06", "a_drop07"});
		}
		else
			award(mission == JoyRide ? 25000 : 10000);
		return true;
	}
	// recharge targets a_drop01/02 (0x42eb8c): both down add a reserve power charge (up to three)
	if (is(caller, "a_drop01") || is(caller, "a_drop02"))
	{
		wave("wave11");
		light(is(caller, "a_drop01") ? "recharge_lower_light" : "recharge_upper_light", true);
		if (down("a_drop01") && down("a_drop02"))
		{
			award(20000);
			if (as.ReserveCharges < 3)
				as.ReserveCharges++;
			update_reserve();
			show(text(3004, "Reserve Power"), 3.0f);
			light("recharge_lower_light", false);
			light("recharge_upper_light", false);
			reset_targets({"a_drop01", "a_drop02"});
		}
		else
			award(6000);
		return true;
	}
	// UFO targets a_drop09-14 (0x42efc8): the bank raises the multiplier (2X, 3X)
	for (int i = 0; i < 6; i++)
		if (is(caller, BodyTargets[i]))
		{
			wave("wave06");
			light(BodyLights[i], true);
			bool all = true;
			for (int k = 0; k < 6; k++)
				all = all && down(BodyTargets[k]);
			if (all)
			{
				award(50000);
				as.UfoStage = (as.UfoStage + 1) % 5;
				if (as.Multiplier < 3)
				{
					as.Multiplier++;
					show(text(3284 + as.Multiplier - 1, std::to_string(as.Multiplier) + "X"), 3.0f);
				}
				for (auto name : BodyLights)
					light(name, false);
				reset_targets({"a_drop09", "a_drop10", "a_drop11", "a_drop12", "a_drop13", "a_drop14"});
			}
			else
				award(UfoScores[as.UfoStage]);
			return true;
		}
	// gear targets a_spot01-04
	for (auto name : GearTargets)
		if (is(caller, name))
		{
			wave("wave07");
			gear_hit(caller);
			return true;
		}
	// cornfield targets a_spot05/06 (0x42fb8c): both light the extra ball
	if (is(caller, "a_spot05") || is(caller, "a_spot06"))
	{
		wave("wave13");
		if (is(caller, "a_spot05"))
			as.CornLeft = true;
		else
			as.CornRight = true;
		light("v_lite97", as.CornLeft);
		light("v_lite98", as.CornRight);
		if (as.CornLeft && as.CornRight)
		{
			as.CornLeft = as.CornRight = false;
			light("v_lite97", false);
			light("v_lite98", false);
			light_extra_ball();
		}
		award(10000);
		return true;
	}

	// ramps
	if (is(caller, "s_ramp15"))
	{
		if (as.MissionOn && as.NextMission == Reassemble && as.Assembled)
			abductee_decided(true);
		else
			landing_ramp(caller);
		return true;
	}
	if (is(caller, "s_ramp13"))
	{
		loop_ramp();
		return true;
	}
	if (is(caller, "s_ramp01"))
	{
		spiral_ramp();
		return true;
	}

	// pockets
	if (is(caller, "s_kout01"))
	{
		if (as.BeamOn) // the tractor beam lifts the ball up to the spacecraft
		{
			wave("wave02");
			wave("wave03");
			timer::set(2.0f, caller, beam_lift);
		}
		else
			eject(caller);
		return true;
	}
	if (is(caller, "stun_kickout"))
		return true; // held by the magnet until fired
	if (is(caller, "s_sink01"))
	{
		auto sink = dynamic_cast<TSink*>(caller);
		bool lockLit = player().Gear[0] && player().Gear[1] && player().Gear[2] && player().Gear[3] && as.Landed;
		if (lockLit && single_ball() && sink)
		{
			spacecraft_lock(sink);
			return true;
		}
		// 0x431ad8: 50,000; every third shot raises the bumpers' value
		wave("wave20");
		award(50000);
		if (++as.SpaceSinkShots >= 3)
		{
			as.SpaceSinkShots = 0;
			wave("wave22");
			if (as.BumperStage < 4)
				as.BumperStage++;
		}
		eject(caller);
		return true;
	}
	if (is(caller, "s_sink02"))
	{
		auto sink = dynamic_cast<TSink*>(caller);
		if (as.MissionOn && as.NextMission == Reassemble) // the tracking device
		{
			show(text(3257, "Tracking Device"), 3.0f);
			award(250000);
			eject(caller);
		}
		else if (sink && !as.Choice)
			offer_choice(as.MissionOn ? 2 : 1, sink);
		else
			eject(caller);
		return true;
	}
	if (is(caller, "corn_sink_1") || is(caller, "corn_sink_2") || is(caller, "corn_sink_3"))
	{
		cornfield(caller);
		eject(caller);
		return true;
	}
	if (is(caller, "power_reserve_intake")) // the outlane kickback uses a charge
	{
		if (as.ReserveCharges > 0)
			as.ReserveCharges--;
		update_reserve();
		award(5000);
		eject(caller, 0.5f);
		return true;
	}
	if (is(caller, "ship_exit_sink") || is(caller, "ship_exit_sink_backup") || is(caller, "power_reserve_outlet"))
	{
		eject(caller);
		return true;
	}
	return false;
}

std::vector<std::string> ft2_alien::Status()
{
	std::vector<std::string> lines;
	if (!table() || as.Table != table())
		return lines;
	if (as.MissionOn && as.NextMission >= 0)
	{
		auto line = std::string("Mission: ") + MissionNames[as.NextMission];
		if (MissionSeconds[as.NextMission] > 0)
			line += " (" + std::to_string(static_cast<int>(as.MissionLeft)) + " s)";
		lines.push_back(line);
		if (as.NextMission == Abduct)
			lines.push_back("Abducted this round: " + std::to_string(as.Abducted) + " of 17");
		if (as.NextMission == Reassemble)
			lines.push_back(as.Assembled ? "Shoot the landing ramp (release) or the loop (keep)"
			                             : "Hit the UFO targets for body parts");
	}
	else if (as.NextMission >= 0)
		lines.push_back(std::string("Next mission: ") + MissionNames[as.NextMission] + " (shoot the cornfield)");
	int done = 0;
	for (auto d : player().MissionDone)
		done += d ? 1 : 0;
	if (done)
		lines.push_back("Missions done: " + std::to_string(done) + " of 5");
	int gear = 0;
	for (auto g : player().Gear)
		gear += g ? 1 : 0;
	if (!as.Landed)
		lines.push_back("Landing gear " + std::to_string(gear) + "/4 (landing ramp lands the ship)");
	else if (gear == 4)
		lines.push_back("Spacecraft lock lit: " + std::to_string(player().Locks) + "/4 balls locked");
	else
		lines.push_back("Ship landed; gear " + std::to_string(gear) + "/4 lights the lock");
	if (as.BeamOn)
		lines.emplace_back("Tractor beam on");
	if (as.StunReady)
		lines.emplace_back("Stun magnet ready");
	if (as.ReserveCharges)
		lines.push_back("Reserve power: " + std::to_string(as.ReserveCharges));
	if (as.Multiplier > 1)
		lines.push_back("Scores x" + std::to_string(as.Multiplier));
	if (as.BonusHold)
		lines.emplace_back("Bonus held");
	if (as.ExtraBallLit)
		lines.emplace_back("Extra ball lit (cornfield)");
	lines.push_back("Jackpot " + commas(as.Jackpot));
	return lines;
}
