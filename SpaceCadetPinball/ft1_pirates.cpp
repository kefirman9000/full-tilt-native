// Skulduggery rules, translated from PIRATES.EXE code segment 23 (see ft1_pirates.h).
//
// Conventions as in ft1_dragon.cpp: each function carries its original address ("23:092a"), data globals keep
// their data-segment address in their name (g_6518), message codes are this engine's names (the Full Tilt 1
// numbers are in ft2/ft1_engine.md), TODO markers name what is not translated yet.

#include "pch.h"
#include "ft1_pirates.h"

#include "control.h"
#include "ft1_strings.h"
#include "ft_log.h"
#include "pb.h"
#include "midi.h"
#include "score.h"
#include "TBlocker.h"
#include "TLight.h"
#include "TLightGroup.h"
#include "TPinballComponent.h"
#include "TPinballTable.h"
#include "TSink.h"
#include "TRollover.h"
#include "TPopupTarget.h"
#include "TSoloTarget.h"
#include "THole.h"
#include "TRamp.h"
#include "TSound.h"
#include "TTextBox.h"
#include "TTimer.h"
#include "TLightSweeps.h"
#include "high_score.h"

namespace
{
	std::vector<component_tag_base*>& TagRegistry()
	{
		static std::vector<component_tag_base*> registry;
		return registry;
	}

	struct TagReg
	{
		explicit TagReg(component_tag_base* tag) { TagRegistry().push_back(tag); }
	};

#define PT_TAG(type, name) component_tag<type> name{#name}; TagReg reg_##name{&name};

	// ---- components (the tags of the data segment) ----
	PT_TAG(TPinballComponent, a_flip3)
	PT_TAG(TPinballComponent, a_flip4)
	PT_TAG(TTextBox, info_text_box)
	PT_TAG(TPinballComponent, a_blocc)
	PT_TAG(TPinballComponent, a_bloce)
	PT_TAG(TPinballComponent, a_blocf)
	PT_TAG(TPinballComponent, a_blocg)
	PT_TAG(TLight, v_lite504)
	PT_TAG(TLight, v_lite505)
	PT_TAG(TLight, v_lite84)
	PT_TAG(TLight, v_lite85)
	PT_TAG(TLight, v_lite1)
	PT_TAG(TLight, v_lite2)
	PT_TAG(TLight, v_lite3)
	PT_TAG(TLight, v_lite4)
	PT_TAG(TLight, v_lite5)
	PT_TAG(TLight, v_lite6)
	PT_TAG(TLight, v_lite7)
	PT_TAG(TLight, v_lite8)
	PT_TAG(TLight, v_lite9)
	PT_TAG(TLight, v_lite10)
	PT_TAG(TLight, v_lite11)
	PT_TAG(TLight, v_lite606)
	PT_TAG(TLight, v_lite607)
	PT_TAG(TLight, v_lite551)
	PT_TAG(TLight, v_lite550)
	PT_TAG(TLight, v_lite549)
	PT_TAG(TLight, v_lite548)
	PT_TAG(TLight, v_lite522)
	PT_TAG(TLight, v_lite779)
	PT_TAG(TLight, v_lite530)
	PT_TAG(TLight, v_lite531)
	PT_TAG(TLight, v_lite532)
	PT_TAG(TLight, v_lite528)
	PT_TAG(TLight, v_lite529)
	PT_TAG(TLight, v_lite524)
	PT_TAG(TLight, v_lite525)
	PT_TAG(TLight, v_lite526)
	PT_TAG(TLight, v_lite527)
	PT_TAG(TLight, v_lite567)
	PT_TAG(TLight, v_lite603)
	PT_TAG(TLight, v_lite604)
	PT_TAG(TLight, v_lite605)
	PT_TAG(TLightGroup, left_island_light_group)
	PT_TAG(TLight, v_lite629)
	PT_TAG(TLight, v_lite630)
	PT_TAG(TLight, v_lite777)
	PT_TAG(TLightGroup, left_island_combo_light_group)
	PT_TAG(TLight, v_lite611)
	PT_TAG(TLight, v_lite612)
	PT_TAG(TLight, v_lite613)
	PT_TAG(TLightGroup, right_wind_light_group)
	PT_TAG(TLight, v_lite626)
	PT_TAG(TLight, v_lite627)
	PT_TAG(TLight, v_lite628)
	PT_TAG(TLightGroup, right_island_combo_light_group)
	PT_TAG(TLight, v_lite600)
	PT_TAG(TLight, v_lite601)
	PT_TAG(TLight, v_lite602)
	PT_TAG(TLightGroup, left_orbit_light_group)
	PT_TAG(TLight, v_lite631)
	PT_TAG(TLight, v_lite632)
	PT_TAG(TLight, v_lite633)
	PT_TAG(TLightGroup, left_orbit_combo_light_group)
	PT_TAG(TLight, v_lite608)
	PT_TAG(TLight, v_lite609)
	PT_TAG(TLight, v_lite610)
	PT_TAG(TLightGroup, right_orbit_light_group)
	PT_TAG(TLight, v_lite634)
	PT_TAG(TLight, v_lite635)
	PT_TAG(TLight, v_lite636)
	PT_TAG(TLightGroup, right_orbit_combo_light_group)
	PT_TAG(TLight, v_lite533)
	PT_TAG(TLight, v_lite534)
	PT_TAG(TLight, v_lite535)
	PT_TAG(TLight, v_lite536)
	PT_TAG(TLight, v_lite537)
	PT_TAG(TLight, v_lite622)
	PT_TAG(TLight, v_lite621)
	PT_TAG(TLight, v_lite620)
	PT_TAG(TLightGroup, south_wind_light_group)
	PT_TAG(TLight, v_lite616)
	PT_TAG(TLight, v_lite615)
	PT_TAG(TLight, v_lite614)
	PT_TAG(TLightGroup, east_wind_1_light_group)
	PT_TAG(TLight, v_lite619)
	PT_TAG(TLight, v_lite618)
	PT_TAG(TLight, v_lite617)
	PT_TAG(TLightGroup, east_wind_2_light_group)
	PT_TAG(TLight, v_lite623)
	PT_TAG(TLight, v_lite624)
	PT_TAG(TLight, v_lite625)
	PT_TAG(TLightGroup, west_wind_light_group)
	PT_TAG(TLight, v_lite538)
	PT_TAG(TLight, v_lite539)
	PT_TAG(TLight, v_lite540)
	PT_TAG(TLightGroup, island_lane_top_light_group)
	PT_TAG(TLight, v_lite541)
	PT_TAG(TLight, v_lite542)
	PT_TAG(TLight, v_lite543)
	PT_TAG(TLightGroup, island_lane_bottom_light_group)
	PT_TAG(TLightGroup, fathom_light_group)
	PT_TAG(TLightGroup, lane_winds_light_group)
	PT_TAG(TPinballComponent, a_kick1)
	PT_TAG(TPinballComponent, s_oneway12)
	PT_TAG(TLight, v_lite568)
	PT_TAG(TLightGroup, coin_light_group)
	PT_TAG(TLight, v_mlite44)
	PT_TAG(TLight, v_mlite107)
	PT_TAG(TLight, v_mlite106)
	PT_TAG(TLight, v_mlite51)
	PT_TAG(TLight, v_mlite110)
	PT_TAG(TLight, v_mlite104)
	PT_TAG(TSound, tabb58wave)
	PT_TAG(TSound, tabb22wave)
	PT_TAG(TSound, tabb26wave)
	PT_TAG(TSound, tabb24wave)
	PT_TAG(TSound, tabb57wave)
	PT_TAG(TSound, tabb59wave)
	PT_TAG(TSound, tabb84wave)
	PT_TAG(TSound, tabb65wave)
	PT_TAG(TLight, v_lite560)
	PT_TAG(TLight, v_lite561)
	PT_TAG(TLight, v_lite562)
	PT_TAG(TLight, v_lite563)
	PT_TAG(TLight, v_lite564)
	PT_TAG(TLight, v_lite565)
	PT_TAG(TLightGroup, keg_light_group)
	PT_TAG(TLight, v_lite571)
	PT_TAG(TLight, v_lite572)
	PT_TAG(TLight, v_lite573)
	PT_TAG(TLight, v_lite574)
	PT_TAG(TLight, v_lite575)
	PT_TAG(TLight, v_lite576)
	PT_TAG(TLightGroup, special_treasure_light_group)
	PT_TAG(TPinballComponent, light_sweeps)
	PT_TAG(TPinballComponent, mode_timer)
	PT_TAG(TSound, tabb42wave)
	PT_TAG(TSound, tabb66wave)
	PT_TAG(TSound, tabb70wave)
	PT_TAG(TSound, tabb11wave)
	PT_TAG(TLight, v_lite778)
	PT_TAG(TSound, tabb67wave)
	PT_TAG(TSound, tabb68wave)
	PT_TAG(TSound, tabb73wave)
	PT_TAG(TSound, tabb74wave)
	PT_TAG(TSound, tabb75wave)
	PT_TAG(TSound, tabb94wave)
	PT_TAG(TSound, tabb32wave)
	PT_TAG(TSound, tabb02wave)
	PT_TAG(TSound, tabb29wave)
	PT_TAG(TSound, tabb12wave)
	PT_TAG(TSound, tabb16wave)
	PT_TAG(TSound, tabb17wave)
	PT_TAG(TSound, tabb19wave)
	PT_TAG(TSound, tabb07wave)
	PT_TAG(TSound, tabb23wave)
	PT_TAG(TSound, tabb27wave)
	PT_TAG(TSound, tabb08wave)
	PT_TAG(TSound, tabb40wave)
	PT_TAG(TSound, tabb41wave)
	PT_TAG(TSound, tabb39wave)
	PT_TAG(TSound, tabb43wave)
	PT_TAG(TSound, tabb61wave)
	PT_TAG(TSound, tabb83wave)
	PT_TAG(TSound, tabb85wave)
	PT_TAG(TSound, tabb86wave)
	PT_TAG(TSound, tabb87wave)
	PT_TAG(TSound, tabb88wave)
	PT_TAG(TLightGroup, map_piece_light_group)
	PT_TAG(TSound, tabb95wave)
	PT_TAG(TSound, tabb99wave)
	PT_TAG(TLight, v_lite552)
	PT_TAG(TLight, v_lite553)
	PT_TAG(TLight, v_lite554)
	PT_TAG(TLight, v_lite555)
	PT_TAG(TLight, v_lite556)
	PT_TAG(TLight, v_lite557)
	PT_TAG(TLight, v_lite558)
	PT_TAG(TLight, v_lite559)
	PT_TAG(TLight, v_lite519)
	PT_TAG(TLight, v_lite520)
	PT_TAG(TLight, v_lite521)
	PT_TAG(TLightGroup, bermuda_light_group)
	PT_TAG(TPinballComponent, s_bwal610)
	PT_TAG(TLight, v_lite570)
	PT_TAG(TSound, tabb00wave)
	PT_TAG(TPinballComponent, a_cannon)
	PT_TAG(TSound, tabb55wave)
	PT_TAG(TSound, tabb28wave)
	PT_TAG(TSound, tabb44wave)
	PT_TAG(TPinballComponent, s_oneway3)
	PT_TAG(TPinballComponent, a_bump1)
	PT_TAG(TPinballComponent, a_bump2)
	PT_TAG(TPinballComponent, a_bump3)
	PT_TAG(TPinballComponent, drain)
	PT_TAG(TPinballComponent, s_launcher1)
	PT_TAG(TLight, v_lite569)
	PT_TAG(TPinballComponent, a_rebo1)
	PT_TAG(TPinballComponent, a_rebo2)
	PT_TAG(TPinballComponent, a_drop5)
	PT_TAG(TPinballComponent, a_drop6)
	PT_TAG(TPinballComponent, a_drop7)
	PT_TAG(TPinballComponent, a_drop8)
	PT_TAG(TPinballComponent, a_drop9)
	PT_TAG(TPinballComponent, a_drop13)
	PT_TAG(TPinballComponent, a_drop14)
	PT_TAG(TPinballComponent, a_drop15)
	PT_TAG(TPinballComponent, a_drop10)
	PT_TAG(TPinballComponent, a_drop11)
	PT_TAG(TPinballComponent, a_drop12)
	PT_TAG(TPinballComponent, a_spot102)
	PT_TAG(TPinballComponent, a_spot11)
	PT_TAG(TPinballComponent, a_spot8)
	PT_TAG(TPinballComponent, a_spot9)
	PT_TAG(TPinballComponent, a_spot10)
	PT_TAG(TPinballComponent, a_spot4)
	PT_TAG(TPinballComponent, a_spot5)
	PT_TAG(TPinballComponent, a_spot6)
	PT_TAG(TPinballComponent, a_sink17)
	PT_TAG(TPinballComponent, a_sink15)
	PT_TAG(TPinballComponent, a_sink14)
	PT_TAG(TPinballComponent, a_sink4)
	PT_TAG(TPinballComponent, a_sink9)
	PT_TAG(TPinballComponent, a_sink12)
	PT_TAG(TPinballComponent, a_sink5)
	PT_TAG(TPinballComponent, a_sink6)
	PT_TAG(TPinballComponent, a_sink7)
	PT_TAG(TPinballComponent, a_ramp8)
	PT_TAG(TPinballComponent, a_ramp5)
	PT_TAG(TPinballComponent, a_ramp13)
	PT_TAG(TPinballComponent, s_trip1)
	PT_TAG(TPinballComponent, s_hole19)
	PT_TAG(TPinballComponent, s_trip2)
	PT_TAG(TPinballComponent, s_rexit8)
	PT_TAG(TPinballComponent, s_trip3)
	PT_TAG(TPinballComponent, s_trip8)
	PT_TAG(TPinballComponent, s_trip11)
	PT_TAG(TPinballComponent, a_roll3)
	PT_TAG(TPinballComponent, a_roll5)
	PT_TAG(TPinballComponent, a_roll1)
	PT_TAG(TPinballComponent, a_roll2)
	PT_TAG(TPinballComponent, a_roll9)
	PT_TAG(TPinballComponent, a_roll8)
	PT_TAG(TPinballComponent, a_roll6)
	PT_TAG(TPinballComponent, a_roll12)
	PT_TAG(TPinballComponent, a_roll11)
	PT_TAG(TPinballComponent, a_roll10)
	PT_TAG(TPinballComponent, a_flip1)
	PT_TAG(TPinballComponent, a_flip2)
	PT_TAG(TPinballComponent, a_kick2)
	PT_TAG(TLight, v_lite544)
	PT_TAG(TPinballComponent, s_oneway7)
	PT_TAG(TLight, v_lite547)
	PT_TAG(TLight, v_lite546)
	PT_TAG(TLight, v_lite545)
	PT_TAG(TPinballComponent, a_sink8)
	PT_TAG(TPinballComponent, a_kout2)
	PT_TAG(TPinballComponent, a_kout1)
	PT_TAG(TPinballComponent, s_magnet1)
	PT_TAG(TPinballComponent, s_magnet2)
	PT_TAG(TPinballComponent, s_magnet3)
	PT_TAG(TPinballComponent, s_magnet4)
	PT_TAG(TPinballComponent, s_magnet5)
	PT_TAG(TPinballComponent, s_magnet6)

	bool Active = false;

	// Full Tilt 1 codes without an equivalent in this engine
	const MessageCode LastHitMovedOn = static_cast<MessageCode>(2077); // FT1 77, to the component that scored last
	const MessageCode FT1Event79 = static_cast<MessageCode>(2079);    // rules-internal events of the mission system
	const MessageCode FT1Event80 = static_cast<MessageCode>(2080);
	const MessageCode FT1Event81 = static_cast<MessageCode>(2081);
	const MessageCode FT1Event82 = static_cast<MessageCode>(2082);
	const MessageCode FT1Event83 = static_cast<MessageCode>(2083);
	const MessageCode FT1LightOverrideOn = MessageCode::TLightFtTmpOverrideOn;    // FT1 24
	const MessageCode FT1LightOverrideOff = MessageCode::TLightFtTmpOverrideOff; // FT1 25
	const MessageCode FT1LightResetOverride = MessageCode::TLightFtResetOverride; // FT1 26

	// ---- engine calls ----
	int Send(component_tag_base& tag, MessageCode code, float value)
	{
		auto comp = tag.GetComponent();
		return comp ? comp->Message(code, value) : 0;
	}

	void Play(component_tag<TSound>& sound)
	{
		if (sound.Component)
			sound.Component->Play(nullptr, "ft1_pirates");
	}

	// 18:1270: the info box's Display; the third argument is the font (1 = the picture font, not loaded yet)
	void Display(const char* text, float time, int font)
	{
		(void)font;
		if (info_text_box.Component)
			info_text_box.Component->Display(text, time);
	}

	void InfoClear() // 18:0b0c
	{
		if (info_text_box.Component)
			info_text_box.Component->Clear();
	}

	int& Field(component_tag_base& tag) // a component's MessageField (+4)
	{
		static int none = 0;
		auto comp = tag.GetComponent();
		return comp ? comp->MessageField : (none = 0);
	}

	int InfoField() { return Field(info_text_box); } // the info box's MessageField: the mission state

	void SetInfoField(int value) { Field(info_text_box) = value; }

	bool LightOn(component_tag<TLight>& light) // +0x3a / +0x40 / +0x3c
	{
		return light.Component && light.Component->light_on();
	}

	int Rand15() // 1:21cd: 0..0x7fff
	{
		return rand() & 0x7fff;
	}

	// 8:16c0 + 1:1dcb: a text of the exe with one number put in
	std::string Format(int textId, int value)
	{
		char buffer[256];
		snprintf(buffer, sizeof buffer, ft1_strings::Get(textId), value);
		return buffer;
	}

	// The original engine's AddScore (15:b8db), as in ft1_dragon.cpp
	int Multiplier = 1;            // table +0x122
	int SuperJackpot = 20000;      // table +0x138
	bool SuperJackpotFlag = false; // table +0x13c

	int AddScore(int score)
	{
		auto t = control::TableG;
		if (t->JackpotScoreFlag)
			t->JackpotScore = std::min(t->JackpotScore + score, 10000000);
		if (SuperJackpotFlag)
			SuperJackpot = std::min(SuperJackpot + score, 20000000);
		if (t->BonusScoreFlag)
			t->BonusScore = std::min(t->BonusScore + score, 5000000);
		auto added = Multiplier * score + t->ScoreAdded;
		t->CurScore += added;
		if (t->CurScore > 1000000000)
		{
			t->CurScore -= 1000000000;
			t->CurScoreE9++;
		}
		score::set(t->CurScoreStruct, t->CurScore);
		return added;
	}

	void ControlHandler(MessageCode code, TPinballComponent* cmp);
	void MissionEvent(MessageCode code, TPinballComponent* cmp);

	TPinballComponent* g_6518 = nullptr; // the component that scored last

	void MovedOn(TPinballComponent* cmp)
	{
		if (g_6518 != cmp)
		{
			if (g_6518)
				ControlHandler(LastHitMovedOn, g_6518);
			g_6518 = cmp;
		}
	}

	// 23:092a - score a hit with the component's score `index`
	int ScoreHit(TPinballComponent* cmp, int index)
	{
		MovedOn(cmp);
		return AddScore(cmp->get_scoring(index));
	}

	// 23:0987 - the same with a given score
	int ScorePoints(TPinballComponent* cmp, int points)
	{
		MovedOn(cmp);
		return AddScore(points);
	}

	// 23:09d3 - a score without bonus, jackpot or multiplier
	int SpecialAddScore(TPinballComponent* cmp, int points)
	{
		auto t = control::TableG;
		auto bonusFlag = t->BonusScoreFlag;     // +0x130
		auto jackpotFlag = t->JackpotScoreFlag; // +0x136
		auto multiplier = Multiplier;           // +0x122
		MovedOn(cmp);
		t->BonusScoreFlag = false;
		t->JackpotScoreFlag = false;
		Multiplier = 1;
		auto added = AddScore(points);
		t->BonusScoreFlag = bonusFlag;
		t->JackpotScoreFlag = jackpotFlag;
		Multiplier = multiplier;
		return added;
	}

	// Music (12:1396 plays, 12:10fe the one playing): FT1 1015 loads tabb1-6 (g_22ce = tabb1 the main tune, g_22d2..
	// g_22e2 tabb2-6, g_22e6 = tabb5 again; a missing one falls back to tabb1); here this engine's tracks
	const int MusicMain = 1;
	int MusicPlaying = 0;

	void PlayMusic(int track)
	{
		if (!midi::play_ft2_track(track))
			midi::play_ft2_track(MusicMain);
		MusicPlaying = track;
	}

	// ---- table messages (23:06e7) ----
	int g_6522 = 0, g_22f2 = 0, g_22f6 = 0, g_6528 = 0, g_652a = 1, g_652c = -1, g_652e = -1, g_6530 = 0,
	    g_6534 = 0, g_6538 = 0, g_6526 = 0;

	void Helper211c(); // 23:211c
	void Helper21be(); // 23:21be
	void Helper0ad6(); // 23:0ad6

	void TableControl(MessageCode code)
	{
		auto t = control::TableG;
		switch (code)
		{
		case MessageCode::SetTiltLock: // FT1 1011
			SetInfoField(17);
			MissionEvent(FT1Event79, nullptr);
			break;
		case MessageCode::NewGame: // FT1 1014
			t->JackpotScore = 1000000; // +0x132
			g_6522 = 0;
			if (auto oneway = s_oneway12.GetComponent())
				oneway->ActiveFlag = 0;
			g_22f2 = 0;
			g_22f6 = 0;
			g_6528 = 0;
			g_652a = 1;
			g_652c = -1;
			g_652e = -1;
			g_6530 = 0;
			g_6534 = 0;
			g_6538 = 0;
			Helper211c();
			Helper21be();
			if (MusicPlaying != MusicMain)
				PlayMusic(MusicMain); // g_22ce
			g_6526 = 0;
			g_6518 = nullptr;
			if (auto kick = a_kick2.GetComponent())
				kick->ActiveFlag = 0;
			if (auto wall = s_bwal610.GetComponent())
				wall->ActiveFlag = 0;
			Send(a_kick1, MessageCode::Ft1Deactivate, 0.0f); // FT1 76
			Send(a_kout1, MessageCode::Ft1Deactivate, 0.0f);
			Send(a_kout2, MessageCode::Ft1Deactivate, 0.0f);
			Helper0ad6();
			SetInfoField(0);
			break;
		default:
			break;
		}
	}


	// ---- the mission system ----
	// The info box's MessageField is the state: 0 game over, 1 ..., 2 normal play, 3 ..., 4-11 the eight map piece
	// missions (their lights 552-559), 12, 13, 16, 17 (tilt) ...; mode_timer's MessageField holds the map mission
	// picked next (1-8).
	int g_6524 = 0; // popup targets hit

	// 23:0d97 - jackpot (+0x132) up, at most 10,000,000
	void AddJackpot(int points)
	{
		auto t = control::TableG;
		t->JackpotScore = std::min(t->JackpotScore + points, 10000000);
	}

	// ds:0x37dc: the state of each map mission, in the order of their lights 552-559
	const int MapMissionState[8] = {4, 8, 7, 5, 6, 9, 10, 11};
	component_tag<TLight>* const MapLights[8] = {&v_lite552, &v_lite553, &v_lite554, &v_lite555,
	                                             &v_lite556, &v_lite557, &v_lite558, &v_lite559};

	// 23:37f2 - the map mission picked next (mode_timer's MessageField 1-8): its light out, none picked
	void ClearPicked()
	{
		auto picked = Field(mode_timer);
		if (picked >= 1 && picked <= 8)
			Send(*MapLights[picked - 1], MessageCode::TLightResetAndTurnOff, 0.0f);
		Field(mode_timer) = 0;
	}

	void MissionLight(int state, bool start);

	// 23:37b9 - map mission n (0-7) is picked: its light flashes
	void Pick(int n)
	{
		if (n >= 8)
			return;
		MissionLight(MapMissionState[n], true);
		Field(mode_timer) = n + 1;
	}

	// 23:3919 - pick the next map mission at random among the lights 552-558 still off (all map pieces lit: 7);
	// none left: the one flashing stays picked
	int PickNext()
	{
		int n = 7;
		if (Send(map_piece_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) !=
			Send(map_piece_light_group, MessageCode::TLightGroupGetLightCount, 0.0f))
		{
			int candidates[7], count = 0;
			for (int i = 0; i < 7; i++)
				if (!LightOn(*MapLights[i]))
					candidates[count++] = i;
			if (!count)
			{
				for (int i = 0; i < 8; i++)
					if (MapLights[i]->Component && MapLights[i]->Component->FlasherOnFlag)
						return i;
				n = 7;
			}
			else
				n = candidates[Rand15() * count / 0x8000];
		}
		ClearPicked();
		Pick(n);
		return n;
	}

	// 23:3676 - a map mission's light: flashing when it starts, on when it is done (state 2: pick the next one)
	void MissionLight(int state, bool start)
	{
		auto code = start ? MessageCode::TLightFlasherStartTimed : MessageCode::TLightResetAndTurnOn;
		ClearPicked();
		switch (state)
		{
		case 2:
			PickNext();
			break;
		case 4: Send(v_lite552, code, -1.0f); break;
		case 5: Send(v_lite555, code, -1.0f); break;
		case 6: Send(v_lite556, code, -1.0f); break;
		case 7: Send(v_lite554, code, -1.0f); break;
		case 8: Send(v_lite553, code, -1.0f); break;
		case 9: Send(v_lite557, code, -1.0f); break;
		case 10: Send(v_lite558, code, -1.0f); break;
		case 11: Send(v_lite559, code, -1.0f); break;
		default: break;
		}
	}

	// ---- light and drop target helpers ----
	void Lights(std::initializer_list<component_tag<TLight>*> lights, MessageCode code, float value)
	{
		for (auto light : lights)
			Send(*light, code, value);
	}

	// 23:1004 - lights 524-527: flashing out over 1 s (and v_lite524's MessageField cleared) or off
	void Lights524(bool flash)
	{
		if (flash)
			Field(v_lite524) = 0;
		Lights({&v_lite524, &v_lite525, &v_lite526, &v_lite527},
		       flash ? MessageCode::TLightFlasherStartTimedThenStayOff : MessageCode::TLightResetAndTurnOff,
		       flash ? 1.0f : 0.0f);
	}

	// 23:1423 - lights 1-11 (the drop targets' lights) the same, v_lite1's MessageField cleared
	void Lights1to11(bool flash)
	{
		if (flash)
			Field(v_lite1) = 0;
		Lights({&v_lite1, &v_lite2, &v_lite3, &v_lite4, &v_lite5, &v_lite6, &v_lite7, &v_lite8, &v_lite9, &v_lite10,
		        &v_lite11},
		       flash ? MessageCode::TLightFlasherStartTimedThenStayOff : MessageCode::TLightResetAndTurnOff,
		       flash ? 1.0f : 0.0f);
	}

	// 23:333a - lights 551-548 the same (a_spot11's MessageField cleared when n is 1)
	void Lights548(int n)
	{
		if (n == 1)
			Field(a_spot11) = 0;
		Lights({&v_lite551, &v_lite550, &v_lite549, &v_lite548},
		       n ? MessageCode::TLightFlasherStartTimedThenStayOff : MessageCode::TLightResetAndTurnOff,
		       n ? 1.0f : 0.0f);
	}

	// 23:10c2 / 23:33f9 - lights 524-527 / 551-548 by the bits of v_lite524's / a_spot11's MessageField
	void BitLights(int bits, std::initializer_list<component_tag<TLight>*> lights)
	{
		int bit = 1;
		for (auto light : lights)
		{
			Send(*light, bits & bit ? MessageCode::TLightResetAndTurnOn : MessageCode::TLightResetAndTurnOff, 0.0f);
			bit <<= 1;
		}
	}

	void Bits524() { BitLights(Field(v_lite524), {&v_lite524, &v_lite525, &v_lite526, &v_lite527}); }
	void Bits548() { BitLights(Field(a_spot11), {&v_lite551, &v_lite550, &v_lite549, &v_lite548}); }

	// 23:1ff6 / 23:2ea5 / 23:32d7 - a bank of drop targets back up (FT1 54), its first one's MessageField cleared
	void DropsUp(std::initializer_list<component_tag_base*> drops)
	{
		Field(**drops.begin()) = 0;
		for (auto drop : drops)
			Send(*drop, MessageCode::TPopupTargetEnable, 0.0f);
	}

	void Drops5Up() { DropsUp({&a_drop5, &a_drop6, &a_drop7, &a_drop8, &a_drop9}); }
	void Drops13Up() { DropsUp({&a_drop13, &a_drop14, &a_drop15}); }
	void Drops10Up() { DropsUp({&a_drop10, &a_drop11, &a_drop12}); }

	// 23:1597 - the drop targets as v_lite1's MessageField keeps them (bit per target: 13-15 bits 0-2, 5-9 bits
	// 3-7, 10-12 bits 8-10): a target down lights its light (1-11) and is marked in its bank's first MessageField;
	// a whole bank down comes back up with its lights on
	void RestoreBank(int mask, int firstBit, std::initializer_list<component_tag<TLight>*> lights,
	                 std::initializer_list<component_tag_base*> drops)
	{
		auto& field = Field(**drops.begin());
		field = 0;
		int count = static_cast<int>(drops.size());
		int all = ((1 << count) - 1) << firstBit;
		auto light = lights.begin();
		auto drop = drops.begin();
		for (int i = 0; i < count; i++, ++light, ++drop)
		{
			if ((mask & all) == all)
			{
				Send(**light, MessageCode::TLightResetAndTurnOn, 0.0f);
				Send(**drop, MessageCode::TPopupTargetEnable, 0.0f);
			}
			else if (mask & 1 << (firstBit + i))
			{
				Send(**light, MessageCode::TLightResetAndTurnOn, 0.0f);
				Send(**drop, MessageCode::TPopupTargetDisable, 0.0f); // FT1 53
				field |= 1 << i;
			}
			else
			{
				Send(**light, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(**drop, MessageCode::TPopupTargetEnable, 0.0f);
			}
		}
	}

	void RestoreDrops()
	{
		auto mask = Field(v_lite1);
		RestoreBank(mask, 0, {&v_lite1, &v_lite2, &v_lite3}, {&a_drop13, &a_drop14, &a_drop15});
		RestoreBank(mask, 3, {&v_lite4, &v_lite5, &v_lite6, &v_lite7, &v_lite8},
		            {&a_drop5, &a_drop6, &a_drop7, &a_drop8, &a_drop9});
		RestoreBank(mask, 8, {&v_lite9, &v_lite10, &v_lite11}, {&a_drop10, &a_drop11, &a_drop12});
	}

	void ReleaseSink8(float time) { Send(a_sink8, MessageCode::TSinkResetTimer, time); } // 23:0dcd

	int g_651c = 0, g_651e = 0;

	// 23:a193 - a map mission starts: lights 524-527 and 1-11 off, a_bloce up after 1 s, v_lite504 on, v_lite505
	// off, lights 548-551 off (not in state 12)
	void MissionStart()
	{
		g_651c = 0;
		g_651e = 0;
		Lights524(false);
		Lights1to11(false);
		Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 1.0f); // FT1 57
		Send(v_lite504, MessageCode::TLightResetAndTurnOn, 0.0f);
		Send(v_lite505, MessageCode::TLightResetAndTurnOff, 0.0f);
		if (InfoField() != 12)
			Lights548(0);
	}

	// 23:a223 - a map mission ends: everything as the player had it
	void MissionEnd()
	{
		Bits524();
		RestoreDrops();
		Bits548();
		Drops5Up();
		Drops13Up();
		Drops10Up();
		Send(v_lite520, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(v_lite521, MessageCode::TLightResetAndTurnOff, 0.0f);
	}

	// The handlers, one per state (23:d8a5's table at cs:da5d)
	int g_6520 = 0; // the skill shot is on
	int g_22f4 = 0;

	void StartState(int state) // the info box gets the state, the mission system event 79
	{
		SetInfoField(state);
		MissionEvent(FT1Event79, nullptr);
	}

	void SkillLightOn() { Send(v_lite606, MessageCode::TLightFlasherStartTimed, -1.0f); } // 23:0a7f
	void SkillLightOff() { Send(v_lite606, MessageCode::TLightResetAndTurnOff, 0.0f); }   // 23:0aac

	void Helper0ad6() // 23:0ad6 - the skill shot (state 3)
	{
		SkillLightOff();
		StartState(3);
	}

	void NormalPlay() // 23:0b07
	{
		SkillLightOff();
		StartState(2);
	}

	// 23:0b38 / 0b83 / 0bce / 0bff / 0c4a - a skill shot that starts a map mission (the picture of it first)
	void SkillMission(int state, const char* picture, float time)
	{
		SkillLightOn();
		if (picture)
			Display(picture, time, 1);
		StartState(state);
	}

	// ds:0x37b0: the coins' worth by the coin lights lit
	const int CoinValue[8] = {0, 50000, 100000, 200000, 400000, 800000, 1600000, 125000};

	// 23:0e93 - a coin: one more coin light, its worth (text 3003), the coin picture; returns `time` plus the time
	// the pictures take
	float AwardCoin(float time)
	{
		Send(coin_light_group, MessageCode::TLightGroupResetAndTurnOn, 1.0f);
		auto lit = Send(coin_light_group, MessageCode::TLightGroupGetOnCount, 1.0f);
		auto points = ScorePoints(nullptr, CoinValue[std::max(0, std::min(7, lit))]);
		auto text = Format(3003, points);
		time += g_22f4 ? 0.25f : 2.0f;
		Display("D", time, 1); // ds:0x225e
		time += g_22f4 ? 0.25f : 2.0f;
		Display(text.c_str(), time, 0);
		Play(tabb84wave);
		return time;
	}

	float Step() { return g_22f4 ? 0.25f : 2.0f; } // ds:0x43f4 / 0x43f8: each award's text 2 s (0.25 when hurried)

	// 23:211c - the six multiball lights off
	void Helper211c()
	{
		for (auto light : {&v_mlite44, &v_mlite107, &v_mlite106, &v_mlite51, &v_mlite110, &v_mlite104})
			Send(*light, MessageCode::TLightResetAndTurnOff, 0.0f);
	}

	// 23:21be - sinks 9 and 12 inactive
	void Helper21be()
	{
		for (auto sink : {&a_sink9, &a_sink12})
			if (auto comp = sink->GetComponent())
				comp->ActiveFlag = 0;
	}

	// 23:2435 - treasure n: special_treasure_light_group's MessageField counts them; 0-4 light 571-575 and are worth
	// 100,000 / 250,000 / 500,000 / 750,000 / 1,000,000 (texts 3019-3023), anything else light 576 and 1,500,000
	// (text 3024)
	float Treasure(int n, float time)
	{
		Field(special_treasure_light_group)++;
		static component_tag<TLight>* const lights[6] = {&v_lite571, &v_lite572, &v_lite573, &v_lite574, &v_lite575,
		                                                 &v_lite576};
		static const int points[6] = {100000, 250000, 500000, 750000, 1000000, 1500000};
		auto index = n >= 0 && n <= 4 ? n : 5;
		auto text = ft1_strings::Get(3019 + index);
		Send(*lights[index], MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		ScorePoints(nullptr, points[index]);
		time += Step();
		Display(text, time, 0);
		Play(tabb84wave);
		return time;
	}

	float TreasureAward(float time) // 23:2627 - the next treasure
	{
		return Treasure(Field(special_treasure_light_group), time);
	}

	// 23:11cb - extra ball (text 3014)
	float ExtraBallAward(float time)
	{
		Play(tabb00wave);
		control::TableG->ExtraBalls = 1; // +0x144
		time += 2.0f;
		Display(ft1_strings::Get(3014), time, 0);
		return time;
	}

	// 23:12ed - a map piece: one more map light (v_lite607's MessageField counts), the map picture, text 3015,
	// 1,000,000, tabb83; all pieces in normal play: the next map mission is picked
	float MapPieceAward(float time)
	{
		Send(map_piece_light_group, MessageCode::TLightGroupResetAndTurnOn, 1.0f);
		Field(v_lite607)++;
		time += 2.0f;
		Display("C", time, 1); // ds:0x225c
		time += 2.0f;
		Display(ft1_strings::Get(3015), time, 0);
		ScorePoints(nullptr, 1000000);
		Play(tabb83wave);
		if (Send(map_piece_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) ==
			Send(map_piece_light_group, MessageCode::TLightGroupGetLightCount, 0.0f) && InfoField() == 2)
			PickNext();
		return time;
	}

	// 23:2f86 - the bumpers at their top value (a_bump2's MessageField 6; text 3044 with a_bump1's 7th score)
	float BumperAward(float time)
	{
		Field(a_bump2) = 6;
		if (time < 0.0f)
			return time;
		auto bump = a_bump1.GetComponent();
		auto text = Format(3044, bump ? bump->get_scoring(6) : 0);
		time += 2.0f;
		Display(text.c_str(), time, 0);
		return time;
	}

	// 23:1ea3 - one more fathom light; the multiplier is the fathoms lit + 1 (its picture, ds:0x2240 + n * 4)
	float FathomAward(float time)
	{
		static const char* pictures[] = {":", ":=<", ":><", ":?<", ":@<", ":A<", ":B<"};
		Send(fathom_light_group, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		auto n = Send(fathom_light_group, MessageCode::TLightGroupGetOnCount, 0.0f);
		if (time >= 0.0f)
		{
			time += 2.0f;
			Display(pictures[std::max(0, std::min(6, n))], time, 1);
		}
		Multiplier = n + 1; // +0x122
		return time;
	}

	// 23:3d4c - the awards collected meanwhile, shown quickly: coins (g_652a), two treasures (g_652c / g_652e; the
	// second reads g_652c too, as the original does), points (g_6530, text 3057), more points (g_6534, text 3058,
	// tabb85), an extra ball (g_6538)
	float PendingAwards(float time)
	{
		g_22f4 = 1;
		g_6528 = 0;
		while (g_652a > 0)
		{
			time = AwardCoin(time);
			g_652a--;
		}
		g_652a = 1;
		if (g_652c != -1)
		{
			time = Treasure(g_652c, time);
			g_652c = -1;
		}
		if (g_652e != -1)
		{
			time = Treasure(g_652c, time);
			g_652e = -1;
		}
		if (g_6530)
		{
			auto text = Format(3057, SpecialAddScore(nullptr, g_6530));
			time += 2.0f;
			Display(text.c_str(), time, 0);
			g_6530 = 0;
		}
		if (g_6534)
		{
			auto text = Format(3058, SpecialAddScore(nullptr, g_6534));
			time += 2.0f;
			Display(text.c_str(), time, 0);
			Play(tabb85wave);
			g_6534 = 0;
		}
		if (g_6538)
		{
			ExtraBallAward(time);
			g_6538 = 0;
		}
		g_22f4 = 0;
		return time;
	}

	// 23:3eec - a random award: one in six each of a treasure, an extra ball, the bumpers, a fathom, a map piece,
	// else the pending awards
	float RandomAward(float time)
	{
		switch (Rand15() * 6 / 0x8000)
		{
		case 0: return TreasureAward(time);
		case 1: return ExtraBallAward(time);
		case 2: return BumperAward(time);
		case 3: return FathomAward(time);
		case 4: return MapPieceAward(time);
		default: return PendingAwards(time);
		}
	}

	// 23:b949 - state 1, the game is over: the table's game over (FT1 1024), tabb5 (g_22e6); when a score makes the
	// high score table (14:0505) tabb73, all the lights flash 3 s and text 3027 shows
	void State1(MessageCode code, TPinballComponent*)
	{
		if (code != FT1Event79)
			return;
		auto t = control::TableG;
		t->Message(MessageCode::GameOver, 0.0f);
		if (MusicPlaying != 5)
			PlayMusic(5);
		bool high = false;
		for (int i = t->PlayerCount - 1; i >= 0 && !high; i--)
		{
			auto score = i == t->CurrentPlayer ? t->CurScore : t->PlayerScores[i].Score;
			high = high_score::get_score_position(score) >= 0;
		}
		if (!high)
			return;
		Play(tabb73wave);
		if (t->LightGroup)
			t->LightGroup->Message(MessageCode::TLightFlasherStartTimedThenStayOff, 3.0f);
		Display(ft1_strings::Get(3027), -1.0f, 0);
	}

	// 23:a27d - state 2, normal play: the main tune; event 80 shows text 3076
	void State2(MessageCode code, TPinballComponent*)
	{
		if (code == FT1Event79)
		{
			g_6520 = 0;
			if (MusicPlaying != MusicMain)
				PlayMusic(MusicMain); // g_22ce
		}
		else if (code == FT1Event80)
			Display(ft1_strings::Get(3076), -1.0f, 0);
	}

	// 23:a319 - state 3, the skill shot: the blockers at the plunger set, v_lite505 on; a picture wheel turns
	// (s_launcher1's MessageField 0-8, one step per 0.2 s, picture strings ds:0x21ce); when the ball passes
	// s_oneway3 the wheel's position is the award
	void State3(MessageCode code, TPinballComponent* cmp)
	{
		static const char* wheel[9] = {"$&(),.2", "\"$&'*,.", "0\"$%(*,", "20\"#&(*", ".20!$&(", ",.2/\"$&",
		                               "*,.10\"$", "(*,-20\"", "&(*+.20"};
		switch (code)
		{
		case FT1Event79:
			g_6520 = 1;
			g_6524 = 0;
			Send(a_blocc, MessageCode::Ft1BlockerSetPosition, -1.0f); // FT1 57
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(a_blocf, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(a_blocg, MessageCode::Ft1BlockerSetPosition, -1.0f);
			break;
		case FT1Event80:
		{
			auto& n = Field(s_launcher1);
			n = n + 1 == 9 ? 0 : n + 1;
			Display(wheel[n], 0.2f, 1);
			break;
		}
		case MessageCode::ControlCollision:
			if (cmp != s_oneway3.GetComponent())
				break;
			InfoClear();
			switch (Field(s_launcher1))
			{
			case 0:
				RandomAward(0.0f);
				NormalPlay();
				break;
			case 1:
			case 2:
			case 8:
				AwardCoin(0.0f);
				NormalPlay();
				break;
			case 3: SkillMission(5, "J", 2.0f); break;   // 23:0b83
			case 4: SkillMission(4, "H", 1.0f); break;   // 23:0b38
			case 5: SkillMission(8, "I", 2.0f); break;   // 23:0c4a
			case 6: SkillMission(7, "333", 2.0f); break; // 23:0bff
			case 7: SkillMission(6, nullptr, 0.0f); break; // 23:0bce
			default: break;
			}
			break;
		default:
			break;
		}
	}
	// The map missions' countdown: event 80 shows the time left (text 3016, 0.5 s); mode_timer running out ends it
	// (event 82); the light sweep done starts it (sink 8 lets its ball go unless the skill shot is on)
	void Countdown()
	{
		auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
		if (left)
			Display(Format(3016, left / 1000).c_str(), 0.5f, 0);
	}

	bool MissionTimers(MessageCode code, TPinballComponent* cmp, float seconds)
	{
		if (code == FT1Event80)
		{
			Countdown();
			return true;
		}
		if (code != MessageCode::ControlTimerExpired || !cmp)
			return false;
		if (dynamic_cast<TTimer*>(cmp)) // 1032
		{
			if (cmp == mode_timer.GetComponent())
				MissionEvent(FT1Event82, nullptr);
			return true;
		}
		if (dynamic_cast<TLightSweeps*>(cmp)) // 1038
		{
			if (!g_6520)
				ReleaseSink8(0.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, seconds);
			MissionEvent(FT1Event80, nullptr);
			return true;
		}
		return false;
	}

	// 23:a54c - state 4, the first map mission (text 3060, tabb2): the light sweep 0, drops 5-9 up, the plunger
	// blockers set, 200,000, lights 4-8 and 567 flash; 29.38 s from the end of the sweep. The fathom lights'
	// timer shows text 3017 with tabb26. Drained: the timer stops and it ends. Its end: v_lite567 off, drops 5-9
	// up, the player's lights back, v_lite606 counts, a_bloce down, v_lite504 off, v_lite505 on
	void State4(MessageCode code, TPinballComponent* cmp)
	{
		switch (code)
		{
		case FT1Event79:
			MissionStart();
			g_651c = 1;
			Send(light_sweeps, MessageCode::Ft1LightSweepStart, 0.0f);
			Drops5Up();
			Send(a_blocc, MessageCode::Ft1BlockerSetPosition, -1.0f);
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 1.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_blocf, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(a_blocg, MessageCode::Ft1BlockerSetPosition, 0.0f);
			ScorePoints(nullptr, 200000);
			Display(ft1_strings::Get(3060), -1.0f, 0);
			Lights({&v_lite4, &v_lite5, &v_lite6, &v_lite7, &v_lite8, &v_lite567},
			       MessageCode::TLightFlasherStartTimed, -1.0f);
			PlayMusic(2); // g_22d2
			Play(tabb58wave);
			break;
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			Send(v_lite567, MessageCode::TLightResetAndTurnOff, 0.0f);
			Drops5Up();
			MissionEnd();
			Field(v_lite606)++;
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			break;
		case MessageCode::ControlNotifyTimerExpired: // FT1 71
			if (cmp == fathom_light_group.GetComponent())
			{
				Display(ft1_strings::Get(3017), 2.0f, 0);
				Play(tabb26wave);
			}
			break;
		default:
			MissionTimers(code, cmp, 29.38f);
			break;
		}
	}
	// 23:3026 - the eight wind and orbit light groups off (and the last hit moved on)
	void WindsOff()
	{
		ScorePoints(nullptr, 0);
		for (auto group : {&left_island_light_group, &right_wind_light_group, &left_orbit_light_group,
		                   &right_orbit_light_group, &south_wind_light_group, &east_wind_1_light_group,
		                   &east_wind_2_light_group, &west_wind_light_group})
			Send(*group, MessageCode::TLightResetAndTurnOff, 0.0f);
	}

	// 23:3105 - the wind or orbit group g_651e (0-7, anything else 0) flashing or on
	void WindLights(bool flash)
	{
		component_tag<TLightGroup>* groups[8] = {&left_orbit_light_group, &right_orbit_light_group,
		                                         &left_island_light_group, &right_wind_light_group,
		                                         &south_wind_light_group, &west_wind_light_group,
		                                         &east_wind_2_light_group, &east_wind_1_light_group};
		if (g_651e < 0 || g_651e > 7)
			g_651e = 0;
		Send(*groups[g_651e], flash ? MessageCode::TLightFlasherStartTimed : MessageCode::TLightResetAndTurnOn,
		     -1.0f);
	}

	int g_3810 = 0; // the Bermuda lights' step (ds:0x3810)

	// 23:ba2b - state 5, the Bermuda Triangle (text 3063, tabb5, tabb66): 200,000, the light sweep 8, the winds out;
	// then the Bermuda lights turn (ds:0x37f0: four sets of four lights, 0.1 s a step), kickout 1 on, the wind or
	// orbit g_651e flashes, 31.14 s (countdown text 3045). Its end: the winds and Bermuda lights out, kickout 1 off
	void State5(MessageCode code, TPinballComponent* cmp)
	{
		static const int sets[4][4] = {{0, 4, 8, 12}, {1, 5, 9, -1}, {2, 6, 10, -1}, {3, 7, 11, -1}};
		switch (code)
		{
		case FT1Event79:
			MissionStart();
			ScorePoints(nullptr, 200000);
			Send(light_sweeps, MessageCode::Ft1LightSweepStart, 8.0f);
			WindsOff();
			PlayMusic(5); // g_22de
			Play(tabb66wave);
			Display(ft1_strings::Get(3063), -1.0f, 0);
			break;
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			WindsOff();
			Send(bermuda_light_group, MessageCode::TLightGroupRestartNotifyTimer, -1.0f); // FT1 47
			Send(bermuda_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_kout1, MessageCode::Ft1Deactivate, 0.0f);
			MissionEnd();
			Field(v_lite606)++;
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			break;
		case FT1Event80:
		{
			auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
			if (left)
				Display(Format(3045, left / 1000).c_str(), 0.5f, 0);
			break;
		}
		case MessageCode::ControlNotifyTimerExpired: // FT1 71
			if (cmp != bermuda_light_group.GetComponent())
				break;
			Send(bermuda_light_group, MessageCode::TLightGroupRestartNotifyTimer, 0.1f);
			for (auto index : sets[g_3810])
				Send(bermuda_light_group, MessageCode::TLightGroupTurnOffAtIndex, static_cast<float>(index)); // FT1 40
			if (++g_3810 > 3)
				g_3810 = 0;
			for (auto index : sets[g_3810])
				Send(bermuda_light_group, MessageCode::TLightGroupTurnOnAtIndex, static_cast<float>(index)); // FT1 39
			break;
		case MessageCode::ControlTimerExpired:
			if (dynamic_cast<TTimer*>(cmp))
			{
				if (cmp == mode_timer.GetComponent())
					MissionEvent(FT1Event82, nullptr);
			}
			else if (dynamic_cast<TLightSweeps*>(cmp))
			{
				if (!g_6520)
					ReleaseSink8(0.0f);
				Send(bermuda_light_group, MessageCode::TLightGroupRestartNotifyTimer, 0.1f);
				Send(a_kout1, MessageCode::Ft1Activate, 0.0f);
				WindLights(true);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 31.14f);
				MissionEvent(FT1Event80, nullptr);
			}
			break;
		default:
			break;
		}
	}
	// 23:239e - the jackpot: tabb85 (time >= 0), its picture ("G") 2 s later, the jackpot (+0x132) scored
	float CollectJackpot(float time)
	{
		if (time >= 0.0f)
			Play(tabb85wave);
		else
			time = -time;
		time += 2.0f;
		Display("G", time, 1); // ds:0x2264
		SpecialAddScore(nullptr, control::TableG->JackpotScore);
		return time;
	}

	// 23:1248 - text 3065 2 s later, v_lite529 lit
	float Award1248(float time)
	{
		time += 2.0f;
		Display(ft1_strings::Get(3065), time, 0);
		Send(v_lite529, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		return time;
	}

	int g_6588 = 8;      // the popup targets needed
	int g_658a = 10000;  // each one's worth
	std::string g_6548; // the countdown text (3046-3049)

	// 23:beac - state 6, the popup targets (tabb6, tabb67): 300,000, the light sweep 9, all drop targets up, lights
	// 1-11 and v_lite778 flash, kickout 2 on. The winds and orbits lit pick how many popup targets are needed and
	// what each is worth (up to 5 lit: 8 x 10,000, text 3046; 6-10: 7 x 25,000, 3047; 11-15: 6 x 50,000, 3048; more:
	// 5 x 100,000, 3049); text 3067; 24.14 s. Each popup target its worth and the count picture ("K" x n); the last
	// one the jackpot. Rollover 6 lights v_lite569 for 10 s. A ball in sink 17, 15 or 14 ends it (text 3050, 1248)
	void State6(MessageCode code, TPinballComponent* cmp)
	{
		static const char* counts[9] = {"", "K", "KK", "KKK", "KKKK", "KKKKK", "KKKKKK", "KKKKKKK", "KKKKKKKK"};
		switch (code)
		{
		case FT1Event79:
		{
			MissionStart();
			ScorePoints(nullptr, 300000);
			Send(light_sweeps, MessageCode::Ft1LightSweepStart, 9.0f);
			PlayMusic(6); // g_22e2
			Play(tabb67wave);
			Send(a_blocc, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(a_blocf, MessageCode::Ft1BlockerSetPosition, -1.0f);
			Send(a_blocg, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Drops13Up();
			Drops5Up();
			Drops10Up();
			Send(v_lite778, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(a_kout2, MessageCode::Ft1Activate, 0.0f);
			Lights({&v_lite1, &v_lite2, &v_lite3, &v_lite4, &v_lite5, &v_lite6, &v_lite7, &v_lite8, &v_lite9,
			        &v_lite10, &v_lite11}, MessageCode::TLightFlasherStartTimed, -1.0f);
			int lit = 0;
			for (auto group : {&left_island_light_group, &right_wind_light_group, &left_orbit_light_group,
			                   &right_orbit_light_group, &south_wind_light_group, &east_wind_1_light_group,
			                   &east_wind_2_light_group, &west_wind_light_group})
				lit += Send(*group, MessageCode::TLightGroupGetOnCount, 0.0f);
			int text;
			if (lit <= 5)
			{
				g_6588 = 8;
				g_658a = 10000;
				text = 3046;
			}
			else if (lit <= 10)
			{
				g_6588 = 7;
				g_658a = 25000;
				text = 3047;
			}
			else if (lit <= 15)
			{
				g_6588 = 6;
				g_658a = 50000;
				text = 3048;
			}
			else
			{
				g_6588 = 5;
				g_658a = 100000;
				text = 3049;
			}
			g_6548 = ft1_strings::Get(text);
			Display(ft1_strings::Get(3067), -1.0f, 0);
			break;
		}
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			Send(v_lite778, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_kout2, MessageCode::Ft1Deactivate, 0.0f);
			MissionEnd();
			Field(v_lite606)++;
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			break;
		case FT1Event80:
		{
			auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
			if (left)
			{
				char buffer[256];
				snprintf(buffer, sizeof buffer, g_6548.c_str(), left / 1000);
				Display(buffer, 0.5f, 0);
			}
			break;
		}
		case MessageCode::ControlCollision:
			if (dynamic_cast<TPopupTarget*>(cmp)) // 1006
			{
				if (++g_651c == g_6588)
				{
					CollectJackpot(0.0f);
					g_651c = 0;
					break;
				}
				Play(tabb68wave);
				ScorePoints(cmp, g_658a);
				Display(counts[std::max(0, std::min(8, g_651c))], 2.0f, 1); // ds:0x227c + n * 9
			}
			else if (dynamic_cast<TRollover*>(cmp)) // 1015
			{
				if (cmp == a_roll6.GetComponent())
				{
					g_6522 = 2;
					Send(v_lite569, MessageCode::TLightTurnOnTimed, 10.0f);
				}
			}
			else if (dynamic_cast<TSink*>(cmp)) // 1017
			{
				if (cmp == a_sink17.GetComponent() || cmp == a_sink15.GetComponent() || cmp == a_sink14.GetComponent())
				{
					Display(ft1_strings::Get(3050), 2.0f, 0);
					State6(FT1Event83, cmp);
					Award1248(2.0f);
				}
			}
			break;
		case MessageCode::ControlTimerExpired:
			if (dynamic_cast<TTimer*>(cmp))
			{
				if (cmp == mode_timer.GetComponent())
				{
					Send(v_lite778, MessageCode::TLightResetAndTurnOff, 0.0f);
					MissionEvent(FT1Event82, nullptr);
				}
			}
			else if (dynamic_cast<TLightSweeps*>(cmp))
			{
				if (!g_6520)
					ReleaseSink8(0.0f);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 24.14f);
				MissionEvent(FT1Event80, nullptr);
			}
			break;
		default:
			break;
		}
	}
	void State7(MessageCode, TPinballComponent*) {}  // TODO(23:adc2)
	void State8(MessageCode, TPinballComponent*) {}  // TODO(23:a93e)
	void State9(MessageCode, TPinballComponent*) {}  // TODO(23:c5cc)
	void State10(MessageCode, TPinballComponent*) {} // TODO(23:c84b)
	void State11(MessageCode, TPinballComponent*) {} // TODO(23:cd08)
	void State12(MessageCode, TPinballComponent*) {} // TODO(23:d2b8)
	void State13(MessageCode, TPinballComponent*) {} // TODO(23:d414)
	void State16(MessageCode, TPinballComponent*) {} // TODO(23:b27e)
	int g_22ee = 0;

	// 23:d63c - state 17, tilt: kicker 2 and wall 610 off, kicker 1 on, oneway 12 off, the magnets and kickouts off,
	// balls kept back (g_6526) back in play; with the last ball draining kicker 1 off
	void State17(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		if (code == FT1Event79)
		{
			g_22ee = 0;
			for (auto part : {&a_kick2, &s_bwal610})
				if (auto comp = part->GetComponent())
					comp->ActiveFlag = 0;
			Send(a_kick1, MessageCode::Ft1Activate, 0.0f); // FT1 75
			if (auto oneway = s_oneway12.GetComponent())
				oneway->ActiveFlag = 0;
			for (auto part : {&s_magnet1, &s_magnet2, &s_magnet3, &s_magnet4, &s_magnet5, &s_magnet6, &a_kout1,
			                  &a_kout2})
				Send(*part, MessageCode::Ft1Deactivate, 0.0f); // FT1 76
			if (g_6526 > 0)
			{
				t->MultiballCount += g_6526;
				g_6526 = 0;
			}
		}
		else if (code == FT1Event83 && t->MultiballCount == 1)
			Send(a_kick1, MessageCode::Ft1Deactivate, 0.0f);
	}

	// 23:d7b6 - any other state: back to normal play
	void StateOther(MessageCode, TPinballComponent*)
	{
		SetInfoField(2);
		MissionEvent(FT1Event79, nullptr);
	}

	// 23:d8a5 - event 79 lights the state's map mission (flashing), event 82 ends it (map missions: jackpot
	// +750,000, the light on, the next one picked); then the state's handler
	void Dispatch(int state, MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event79)
			MissionLight(state, true);
		else if (code == FT1Event82)
		{
			if (state >= 4 && state <= 11)
				AddJackpot(750000);
			MissionLight(state, false);
			PickNext();
		}
		switch (state)
		{
		case 0: break;
		case 1: State1(code, cmp); break;
		case 2: State2(code, cmp); break;
		case 3: State3(code, cmp); break;
		case 4: State4(code, cmp); break;
		case 5: State5(code, cmp); break;
		case 6: State6(code, cmp); break;
		case 7: State7(code, cmp); break;
		case 8: State8(code, cmp); break;
		case 9: State9(code, cmp); break;
		case 10: State10(code, cmp); break;
		case 11: State11(code, cmp); break;
		case 12: State12(code, cmp); break;
		case 13: State13(code, cmp); break;
		case 16: State16(code, cmp); break;
		case 17: State17(code, cmp); break;
		default: StateOther(code, cmp); break;
		}
	}

	// 23:d7e3 - the mission system hears every message: popup targets (count in g_6524) and solo targets add
	// 20,000 to the jackpot, sinks 40,000; the info box's own timer with nothing queued (+0x4e) is event 80; after
	// event 82 back to normal play
	bool TextQueued() { return false; } // TODO(+0x4e): this engine's text box has no queue flag

	void MissionEvent(MessageCode code, TPinballComponent* cmp)
	{
		if (!info_text_box.Component)
			return;
		if (cmp)
		{
			if (dynamic_cast<TPopupTarget*>(cmp)) // 1006
			{
				g_6524++;
				AddJackpot(20000);
			}
			else if (dynamic_cast<TSoloTarget*>(cmp)) // 1019
				AddJackpot(20000);
			else if (dynamic_cast<TSink*>(cmp)) // 1017
				AddJackpot(40000);
		}
		auto state = InfoField();
		if (code == MessageCode::ControlTimerExpired && cmp == info_text_box.GetComponent() && !TextQueued())
			code = FT1Event80;
		Dispatch(state, code, cmp);
		if (code == FT1Event82)
		{
			SetInfoField(2);
			MissionEvent(FT1Event79, nullptr);
		}
	}

	// ---- rule functions not translated yet ----
	void Rule_98d7(MessageCode, TPinballComponent*) {} // TODO(23:98d7): s_oneway3
	void Rule_472d(MessageCode, TPinballComponent*) {} // TODO(23:472d): a_bump1, a_bump2, a_bump3
	void Rule_4448(MessageCode, TPinballComponent*) {} // TODO(23:4448): drain
	void Rule_4152(MessageCode, TPinballComponent*) {} // TODO(23:4152): s_launcher1
	void Rule_43b9(MessageCode, TPinballComponent*) {} // TODO(23:43b9): v_lite569
	void Rule_5650(MessageCode, TPinballComponent*) {} // TODO(23:5650): a_rebo1
	void Rule_56af(MessageCode, TPinballComponent*) {} // TODO(23:56af): a_rebo2
	void Rule_4b23(MessageCode, TPinballComponent*) {} // TODO(23:4b23): a_drop5
	void Rule_4ba8(MessageCode, TPinballComponent*) {} // TODO(23:4ba8): a_drop6
	void Rule_4c2d(MessageCode, TPinballComponent*) {} // TODO(23:4c2d): a_drop7
	void Rule_4cb2(MessageCode, TPinballComponent*) {} // TODO(23:4cb2): a_drop8
	void Rule_4d37(MessageCode, TPinballComponent*) {} // TODO(23:4d37): a_drop9
	void Rule_5076(MessageCode, TPinballComponent*) {} // TODO(23:5076): a_drop13
	void Rule_50f7(MessageCode, TPinballComponent*) {} // TODO(23:50f7): a_drop14
	void Rule_517c(MessageCode, TPinballComponent*) {} // TODO(23:517c): a_drop15
	void Rule_54b8(MessageCode, TPinballComponent*) {} // TODO(23:54b8): a_drop10
	void Rule_5540(MessageCode, TPinballComponent*) {} // TODO(23:5540): a_drop11
	void Rule_55c8(MessageCode, TPinballComponent*) {} // TODO(23:55c8): a_drop12
	void Rule_570e(MessageCode, TPinballComponent*) {} // TODO(23:570e): a_spot102
	void Rule_59f4(MessageCode, TPinballComponent*) {} // TODO(23:59f4): a_spot11
	void Rule_5b27(MessageCode, TPinballComponent*) {} // TODO(23:5b27): a_spot8
	void Rule_5c6a(MessageCode, TPinballComponent*) {} // TODO(23:5c6a): a_spot9
	void Rule_5d9d(MessageCode, TPinballComponent*) {} // TODO(23:5d9d): a_spot10
	void Rule_5f7f(MessageCode, TPinballComponent*) {} // TODO(23:5f7f): a_spot4
	void Rule_5fe2(MessageCode, TPinballComponent*) {} // TODO(23:5fe2): a_spot5
	void Rule_6049(MessageCode, TPinballComponent*) {} // TODO(23:6049): a_spot6
	void Rule_62f4(MessageCode, TPinballComponent*) {} // TODO(23:62f4): a_sink17
	void Rule_65a6(MessageCode, TPinballComponent*) {} // TODO(23:65a6): a_sink15
	void Rule_6873(MessageCode, TPinballComponent*) {} // TODO(23:6873): a_sink14
	void Rule_6b40(MessageCode, TPinballComponent*) {} // TODO(23:6b40): a_sink4
	void Rule_6eef(MessageCode, TPinballComponent*) {} // TODO(23:6eef): a_sink9
	void Rule_7061(MessageCode, TPinballComponent*) {} // TODO(23:7061): a_sink12
	void Rule_734c(MessageCode, TPinballComponent*) {} // TODO(23:734c): a_sink5
	void Rule_742b(MessageCode, TPinballComponent*) {} // TODO(23:742b): a_sink6
	void Rule_750e(MessageCode, TPinballComponent*) {} // TODO(23:750e): a_sink7
	void Rule_75f1(MessageCode, TPinballComponent*) {} // TODO(23:75f1): a_ramp8
	void Rule_78dd(MessageCode, TPinballComponent*) {} // TODO(23:78dd): a_ramp5
	void Rule_7bab(MessageCode, TPinballComponent*) {} // TODO(23:7bab): a_ramp13
	void Rule_7c6f(MessageCode, TPinballComponent*) {} // TODO(23:7c6f): s_trip1
	void Rule_7cf1(MessageCode, TPinballComponent*) {} // TODO(23:7cf1): s_hole19
	void Rule_7dc4(MessageCode, TPinballComponent*) {} // TODO(23:7dc4): s_trip2
	void Rule_7e9b(MessageCode, TPinballComponent*) {} // TODO(23:7e9b): s_rexit8
	void Rule_7fb9(MessageCode, TPinballComponent*) {} // TODO(23:7fb9): s_trip3
	void Rule_8287(MessageCode, TPinballComponent*) {} // TODO(23:8287): s_trip8
	void Rule_858e(MessageCode, TPinballComponent*) {} // TODO(23:858e): s_trip11
	void Rule_865a(MessageCode, TPinballComponent*) {} // TODO(23:865a): a_roll3
	void Rule_8751(MessageCode, TPinballComponent*) {} // TODO(23:8751): a_roll5
	void Rule_883b(MessageCode, TPinballComponent*) {} // TODO(23:883b): a_roll1
	void Rule_8925(MessageCode, TPinballComponent*) {} // TODO(23:8925): a_roll2
	void Rule_8b78(MessageCode, TPinballComponent*) {} // TODO(23:8b78): a_roll9
	void Rule_8d6c(MessageCode, TPinballComponent*) {} // TODO(23:8d6c): a_roll8
	void Rule_8f60(MessageCode, TPinballComponent*) {} // TODO(23:8f60): a_roll6
	void Rule_9218(MessageCode, TPinballComponent*) {} // TODO(23:9218): a_roll12
	void Rule_9347(MessageCode, TPinballComponent*) {} // TODO(23:9347): a_roll11
	void Rule_9476(MessageCode, TPinballComponent*) {} // TODO(23:9476): a_roll10
	void Rule_95a5(MessageCode, TPinballComponent*) {} // TODO(23:95a5): a_flip1
	void Rule_9624(MessageCode, TPinballComponent*) {} // TODO(23:9624): a_flip2
	void Rule_971d(MessageCode, TPinballComponent*) {} // TODO(23:971d): a_kick2
	void Rule_96a3(MessageCode, TPinballComponent*) {} // TODO(23:96a3): v_lite544
	void Rule_9813(MessageCode, TPinballComponent*) {} // TODO(23:9813): s_oneway7
	void Rule_0000(MessageCode, TPinballComponent*) {} // TODO(23:0000): v_lite547, v_lite546, v_lite545
	void Rule_9922(MessageCode, TPinballComponent*) {} // TODO(23:9922): a_sink8
	void Rule_999c(MessageCode, TPinballComponent*) {} // TODO(23:999c): a_kout2, a_kout1
	void Rule_9a2e(MessageCode, TPinballComponent*) {} // TODO(23:9a2e): s_magnet1
	void Rule_9aff(MessageCode, TPinballComponent*) {} // TODO(23:9aff): s_magnet2
	void Rule_9bd0(MessageCode, TPinballComponent*) {} // TODO(23:9bd0): s_magnet3
	void Rule_9ca1(MessageCode, TPinballComponent*) {} // TODO(23:9ca1): s_magnet4
	void Rule_9d72(MessageCode, TPinballComponent*) {} // TODO(23:9d72): s_magnet5
	void Rule_9e43(MessageCode, TPinballComponent*) {} // TODO(23:9e43): s_magnet6

	// ds:0x2c18 - the rule table (23:05ad links it): each component's rule function and scores
	const int Scores1[] = {1500, 3000, 4500, 6000, 7500, 9000, 10500};
	const int Scores2[] = {1500, 3000, 4500, 6000, 7500, 9000, 10500};
	const int Scores3[] = {1500, 3000, 4500, 6000, 7500, 9000, 10500};
	const int Scores7[] = {350};
	const int Scores8[] = {350};
	const int Scores9[] = {3000, 25000};
	const int Scores10[] = {3000, 25000};
	const int Scores11[] = {3000, 25000};
	const int Scores12[] = {3000, 25000};
	const int Scores13[] = {3000, 25000};
	const int Scores14[] = {4000, 30000};
	const int Scores15[] = {4000, 30000};
	const int Scores16[] = {4000, 30000};
	const int Scores17[] = {5000, 35000};
	const int Scores18[] = {5000, 35000};
	const int Scores19[] = {5000, 35000};
	const int Scores20[] = {50000};
	const int Scores21[] = {1000, 15000};
	const int Scores22[] = {1000, 15000};
	const int Scores23[] = {1000, 15000};
	const int Scores24[] = {1000, 15000};
	const int Scores25[] = {5000, 20000};
	const int Scores26[] = {5000, 20000};
	const int Scores27[] = {5000};
	const int Scores28[] = {40000};
	const int Scores29[] = {40000};
	const int Scores30[] = {40000};
	const int Scores31[] = {100000};
	const int Scores32[] = {40000};
	const int Scores33[] = {40000};
	const int Scores34[] = {75000, 200000};
	const int Scores35[] = {75000, 200000};
	const int Scores36[] = {75000, 200000};
	const int Scores37[] = {25000, 50000, 75000, 150000, 175000, 200000, 275000, 300000, 325000, 425000, 450000, 475000};
	const int Scores38[] = {25000, 50000, 75000, 150000, 175000, 200000, 275000, 300000, 325000, 425000, 450000, 475000};
	const int Scores39[] = {50000};
	const int Scores43[] = {100000};
	const int Scores44[] = {25000, 50000, 75000, 150000, 175000, 200000, 275000, 300000, 325000, 425000, 450000, 475000};
	const int Scores45[] = {25000, 50000, 75000, 150000, 175000, 200000, 275000, 300000, 325000, 425000, 450000, 475000};
	const int Scores46[] = {25000};
	const int Scores47[] = {25000};
	const int Scores48[] = {10000};
	const int Scores49[] = {10000};
	const int Scores50[] = {25000, 50000, 75000};
	const int Scores51[] = {25000, 50000, 75000};
	const int Scores52[] = {25000, 50000, 75000};
	const int Scores53[] = {25000, 50000, 75000};
	const int Scores54[] = {1200, 20000};
	const int Scores55[] = {1200, 20000};
	const int Scores56[] = {1200, 20000};
	const int Scores59[] = {5000};
	component_tag<> RuleTags[] = {{"s_oneway3"}, {"a_bump1"}, {"a_bump2"}, {"a_bump3"}, {"drain"}, {"s_launcher1"}, {"v_lite569"}, {"a_rebo1"}, {"a_rebo2"}, {"a_drop5"}, {"a_drop6"}, {"a_drop7"}, {"a_drop8"}, {"a_drop9"}, {"a_drop13"}, {"a_drop14"}, {"a_drop15"}, {"a_drop10"}, {"a_drop11"}, {"a_drop12"}, {"a_spot102"}, {"a_spot11"}, {"a_spot8"}, {"a_spot9"}, {"a_spot10"}, {"a_spot4"}, {"a_spot5"}, {"a_spot6"}, {"a_sink17"}, {"a_sink15"}, {"a_sink14"}, {"a_sink4"}, {"a_sink9"}, {"a_sink12"}, {"a_sink5"}, {"a_sink6"}, {"a_sink7"}, {"a_ramp8"}, {"a_ramp5"}, {"a_ramp13"}, {"s_trip1"}, {"s_hole19"}, {"s_trip2"}, {"s_rexit8"}, {"s_trip3"}, {"s_trip8"}, {"s_trip11"}, {"a_roll3"}, {"a_roll5"}, {"a_roll1"}, {"a_roll2"}, {"a_roll9"}, {"a_roll8"}, {"a_roll6"}, {"a_roll12"}, {"a_roll11"}, {"a_roll10"}, {"a_flip1"}, {"a_flip2"}, {"a_kick2"}, {"v_lite544"}, {"s_oneway7"}, {"v_lite547"}, {"v_lite546"}, {"v_lite545"}, {"a_sink8"}, {"a_kout2"}, {"a_kout1"}, {"s_magnet1"}, {"s_magnet2"}, {"s_magnet3"}, {"s_magnet4"}, {"s_magnet5"}, {"s_magnet6"}};
	component_info Rules[] = {
		{RuleTags[0], {Rule_98d7, 0, nullptr}},
		{RuleTags[1], {Rule_472d, 7, Scores1}},
		{RuleTags[2], {Rule_472d, 7, Scores2}},
		{RuleTags[3], {Rule_472d, 7, Scores3}},
		{RuleTags[4], {Rule_4448, 0, nullptr}},
		{RuleTags[5], {Rule_4152, 0, nullptr}},
		{RuleTags[6], {Rule_43b9, 0, nullptr}},
		{RuleTags[7], {Rule_5650, 1, Scores7}},
		{RuleTags[8], {Rule_56af, 1, Scores8}},
		{RuleTags[9], {Rule_4b23, 2, Scores9}},
		{RuleTags[10], {Rule_4ba8, 2, Scores10}},
		{RuleTags[11], {Rule_4c2d, 2, Scores11}},
		{RuleTags[12], {Rule_4cb2, 2, Scores12}},
		{RuleTags[13], {Rule_4d37, 2, Scores13}},
		{RuleTags[14], {Rule_5076, 2, Scores14}},
		{RuleTags[15], {Rule_50f7, 2, Scores15}},
		{RuleTags[16], {Rule_517c, 2, Scores16}},
		{RuleTags[17], {Rule_54b8, 2, Scores17}},
		{RuleTags[18], {Rule_5540, 2, Scores18}},
		{RuleTags[19], {Rule_55c8, 2, Scores19}},
		{RuleTags[20], {Rule_570e, 1, Scores20}},
		{RuleTags[21], {Rule_59f4, 2, Scores21}},
		{RuleTags[22], {Rule_5b27, 2, Scores22}},
		{RuleTags[23], {Rule_5c6a, 2, Scores23}},
		{RuleTags[24], {Rule_5d9d, 2, Scores24}},
		{RuleTags[25], {Rule_5f7f, 2, Scores25}},
		{RuleTags[26], {Rule_5fe2, 2, Scores26}},
		{RuleTags[27], {Rule_6049, 1, Scores27}},
		{RuleTags[28], {Rule_62f4, 1, Scores28}},
		{RuleTags[29], {Rule_65a6, 1, Scores29}},
		{RuleTags[30], {Rule_6873, 1, Scores30}},
		{RuleTags[31], {Rule_6b40, 1, Scores31}},
		{RuleTags[32], {Rule_6eef, 1, Scores32}},
		{RuleTags[33], {Rule_7061, 1, Scores33}},
		{RuleTags[34], {Rule_734c, 2, Scores34}},
		{RuleTags[35], {Rule_742b, 2, Scores35}},
		{RuleTags[36], {Rule_750e, 2, Scores36}},
		{RuleTags[37], {Rule_75f1, 12, Scores37}},
		{RuleTags[38], {Rule_78dd, 12, Scores38}},
		{RuleTags[39], {Rule_7bab, 1, Scores39}},
		{RuleTags[40], {Rule_7c6f, 0, nullptr}},
		{RuleTags[41], {Rule_7cf1, 0, nullptr}},
		{RuleTags[42], {Rule_7dc4, 0, nullptr}},
		{RuleTags[43], {Rule_7e9b, 1, Scores43}},
		{RuleTags[44], {Rule_7fb9, 12, Scores44}},
		{RuleTags[45], {Rule_8287, 12, Scores45}},
		{RuleTags[46], {Rule_858e, 1, Scores46}},
		{RuleTags[47], {Rule_865a, 1, Scores47}},
		{RuleTags[48], {Rule_8751, 1, Scores48}},
		{RuleTags[49], {Rule_883b, 1, Scores49}},
		{RuleTags[50], {Rule_8925, 3, Scores50}},
		{RuleTags[51], {Rule_8b78, 3, Scores51}},
		{RuleTags[52], {Rule_8d6c, 3, Scores52}},
		{RuleTags[53], {Rule_8f60, 3, Scores53}},
		{RuleTags[54], {Rule_9218, 2, Scores54}},
		{RuleTags[55], {Rule_9347, 2, Scores55}},
		{RuleTags[56], {Rule_9476, 2, Scores56}},
		{RuleTags[57], {Rule_95a5, 0, nullptr}},
		{RuleTags[58], {Rule_9624, 0, nullptr}},
		{RuleTags[59], {Rule_971d, 1, Scores59}},
		{RuleTags[60], {Rule_96a3, 0, nullptr}},
		{RuleTags[61], {Rule_9813, 0, nullptr}},
		{RuleTags[62], {Rule_0000, 0, nullptr}},
		{RuleTags[63], {Rule_0000, 0, nullptr}},
		{RuleTags[64], {Rule_0000, 0, nullptr}},
		{RuleTags[65], {Rule_9922, 0, nullptr}},
		{RuleTags[66], {Rule_999c, 0, nullptr}},
		{RuleTags[67], {Rule_999c, 0, nullptr}},
		{RuleTags[68], {Rule_9a2e, 0, nullptr}},
		{RuleTags[69], {Rule_9aff, 0, nullptr}},
		{RuleTags[70], {Rule_9bd0, 0, nullptr}},
		{RuleTags[71], {Rule_9ca1, 0, nullptr}},
		{RuleTags[72], {Rule_9d72, 0, nullptr}},
		{RuleTags[73], {Rule_9e43, 0, nullptr}},
	};

	// 23:066f - the rules' message handler: the mission system hears every message first, then the component's rule
	void ControlHandler(MessageCode code, TPinballComponent* cmp)
	{
		MissionEvent(code, cmp);
		if (!cmp || !cmp->Control)
			return;
		cmp->Control->ControlFunc(code, cmp);
	}
}

bool ft1_pirates::Wanted()
{
	return pb::DevDebug && getenv("FT1_PIRATES_FULL");
}

bool ft1_pirates::Link(TPinballTable* table)
{
	(void)table;
	Clear();
	for (auto tag : TagRegistry())
		if (!control::make_component_link(*tag))
			ftlog::Line(std::string("FT1 Skulduggery rules: component missing on the table: ") + tag->Name);
	int linked = 0;
	for (auto& rule : Rules)
		if (auto comp = control::make_component_link(rule.Tag))
		{
			comp->Control = &rule.Control;
			linked++;
		}
		else
			ftlog::Line(std::string("FT1 Skulduggery rules: rule component missing: ") + rule.Tag.Name);
	TRamp::Ft1Events = true;
	Active = true;
	ftlog::Line("FT1 Skulduggery: translation active (work in progress), " + std::to_string(linked) + " rule entries");
	return Active;
}

void ft1_pirates::Clear()
{
	Active = false;
	g_6518 = nullptr;
	Multiplier = 1;
	SuperJackpot = 20000;
	SuperJackpotFlag = false;
}

bool ft1_pirates::Handle(MessageCode code, TPinballComponent* cmp)
{
	if (!Active)
		return false;
	ControlHandler(code, cmp);
	return true;
}

bool ft1_pirates::TableHandler(MessageCode code)
{
	if (!Active)
		return false;
	TableControl(code);
	return true;
}
