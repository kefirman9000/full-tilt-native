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
#include "TLightRollover.h"
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
	// 23:3fcf - lights 530-532 (on and then flashing out over 1 s) or off; a_spot6's MessageField cleared
	void Lights530(bool flash)
	{
		auto code = flash ? MessageCode::TLightFlasherStartTimedThenStayOff : MessageCode::TLightResetAndTurnOff;
		if (flash)
			Lights({&v_lite530, &v_lite531, &v_lite532}, MessageCode::TLightTurnOn, 0.0f);
		Lights({&v_lite530, &v_lite531, &v_lite532}, code, 1.0f);
		Field(a_spot6) = 0;
	}

	// 23:2a00 - the six magnets and their lights (560-565) off
	void MagnetsOff()
	{
		component_tag_base* magnets[6] = {&s_magnet1, &s_magnet2, &s_magnet3, &s_magnet4, &s_magnet5, &s_magnet6};
		component_tag<TLight>* lights[6] = {&v_lite560, &v_lite561, &v_lite562, &v_lite563, &v_lite564, &v_lite565};
		for (int i = 0; i < 6; i++)
		{
			Send(*magnets[i], MessageCode::Ft1Deactivate, 0.0f);
			Send(*lights[i], MessageCode::TLightResetAndTurnOff, 0.0f);
		}
	}

	// 23:adc2 - state 7 (text 3062, tabb4, tabb65): 200,000, the light sweep 7, drops 13-15 up, lights 1-3 flash;
	// then lights 545-547 blink at random (one of them flashes 0.1-0.25 s, then the next), 27.4 s (countdown text
	// 3025). Its end: the magnets off
	void State7(MessageCode code, TPinballComponent* cmp)
	{
		switch (code)
		{
		case FT1Event79:
			MissionStart();
			Lights530(false);
			ScorePoints(nullptr, 200000);
			Send(light_sweeps, MessageCode::Ft1LightSweepStart, 7.0f);
			PlayMusic(4); // g_22da
			Play(tabb65wave);
			Drops13Up();
			Lights({&v_lite547, &v_lite546, &v_lite545}, MessageCode::TLightResetAndTurnOff, 0.0f);
			Field(a_spot6) = 0;
			Lights({&v_lite530, &v_lite531, &v_lite532}, MessageCode::TLightResetAndTurnOff, 0.0f);
			Display(ft1_strings::Get(3062), -1.0f, 0);
			Lights({&v_lite1, &v_lite2, &v_lite3}, MessageCode::TLightFlasherStartTimed, -1.0f);
			break;
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			Lights530(false);
			Drops13Up();
			MagnetsOff();
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
				Display(Format(3025, left / 1000).c_str(), 0.5f, 0);
			break;
		}
		case MessageCode::ControlTimerExpired:
			if (dynamic_cast<TLight*>(cmp)) // 1002
			{
				if (cmp != v_lite547.GetComponent() && cmp != v_lite546.GetComponent() &&
					cmp != v_lite545.GetComponent())
					break;
				component_tag<TLight>* lights[3] = {&v_lite547, &v_lite546, &v_lite545};
				auto which = Rand15() * 3 / 0x8000;
				auto time = static_cast<float>(Rand15() * 150 / 0x8000 + 100) / 1000.0f; // ds:0x43fc
				Send(*lights[which > 2 ? 2 : which], MessageCode::TLightFlasherStartTimed, time);
			}
			else if (dynamic_cast<TTimer*>(cmp))
			{
				if (cmp != mode_timer.GetComponent())
					break;
				Lights({&v_lite547, &v_lite546, &v_lite545}, MessageCode::TLightResetAndTurnOff, 0.0f);
				MissionEvent(FT1Event82, nullptr);
			}
			else if (dynamic_cast<TLightSweeps*>(cmp))
			{
				if (!g_6520)
					ReleaseSink8(0.0f);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 27.4f);
				MissionEvent(FT1Event80, nullptr);
				State7(MessageCode::ControlTimerExpired, v_lite547.GetComponent());
			}
			break;
		default:
			break;
		}
	}
	// 23:21e2 - sinks 9 and 12 on; the next multiball shot (g_651e 0-3, then back to the first) flashes its lights;
	// returns its light sweep (5, 2, 6, 3, else 1)
	int NextMultiShot()
	{
		for (auto sink : {&a_sink9, &a_sink12})
			if (auto comp = sink->GetComponent())
				comp->ActiveFlag = 1;
		auto flash = [](component_tag_base& part) { Send(part, MessageCode::TLightFlasherStartTimed, -1.0f); };
		switch (g_651e++)
		{
		case 0:
			flash(v_mlite110);
			flash(left_orbit_light_group);
			return 5;
		case 1:
			flash(v_mlite107);
			flash(v_lite525);
			flash(v_lite520);
			return 2;
		case 2:
			flash(v_mlite104);
			flash(right_orbit_light_group);
			return 6;
		case 3:
			flash(v_mlite106);
			flash(v_lite526);
			flash(v_lite521);
			return 3;
		default:
			g_651e = 0;
			flash(v_mlite44);
			flash(v_lite524);
			flash(v_lite519);
			return 1;
		}
	}

	// 23:a93e - state 8 (text 3061, tabb3, tabb59): the plunger blockers away, 300,000, the multiball lights out,
	// tripwires 3 and 8 off keeping the orbit lights lit (their MessageFields), the orbits and combos out; the next
	// multiball shot flashes and its light sweep runs; 30.68 s (countdown text 3053). Light rollovers add 20,000 to
	// the jackpot. Its end: tripwires 3 and 8 back on with their orbit lights
	void State8(MessageCode code, TPinballComponent* cmp)
	{
		switch (code)
		{
		case FT1Event79:
			MissionStart();
			for (auto blocker : {&a_blocc, &a_blocf, &a_blocg})
				Send(*blocker, MessageCode::Ft1BlockerSetPosition, -1.0f);
			ScorePoints(nullptr, 300000);
			Helper211c();
			for (auto trip : {&s_trip3, &s_trip8})
				if (auto comp = trip->GetComponent())
					comp->ActiveFlag = 0;
			Field(s_trip3) = Send(left_orbit_light_group, MessageCode::TLightGroupGetOnCount, 0.0f);
			Field(s_trip8) = Send(right_orbit_light_group, MessageCode::TLightGroupGetOnCount, 0.0f);
			for (auto group : {&left_orbit_light_group, &right_orbit_light_group, &left_orbit_combo_light_group,
			                   &right_orbit_combo_light_group})
				Send(*group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite520, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite521, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(light_sweeps, MessageCode::Ft1LightSweepStart, static_cast<float>(NextMultiShot()));
			PlayMusic(3); // g_22d6
			Play(tabb59wave);
			Display(ft1_strings::Get(3061), -1.0f, 0);
			break;
		case MessageCode::ControlCollision:
			if (dynamic_cast<TLightRollover*>(cmp)) // 1020
				AddJackpot(20000);
			break;
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			Helper211c();
			Helper21be();
			MissionEnd();
			Field(v_lite606)++;
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(v_lite519, MessageCode::TLightResetAndTurnOff, 0.0f);
			for (auto trip : {&s_trip3, &s_trip8})
				if (auto comp = trip->GetComponent())
					comp->ActiveFlag = 1;
			Send(left_orbit_light_group, MessageCode::TLightTurnOff, 0.0f);
			Send(right_orbit_light_group, MessageCode::TLightTurnOff, 0.0f);
			Send(left_orbit_light_group, MessageCode::TLightGroupToggleSplitIndex, static_cast<float>(Field(s_trip3)));
			Send(right_orbit_light_group, MessageCode::TLightGroupToggleSplitIndex,
			     static_cast<float>(Field(s_trip8))); // FT1 49
			break;
		case FT1Event80:
		{
			auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
			if (left)
				Display(Format(3053, left / 1000).c_str(), 0.5f, 0);
			break;
		}
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
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.68f);
				MissionEvent(FT1Event80, nullptr);
			}
			break;
		default:
			break;
		}
	}
	// The end and countdown shared by states 9 and 10 (and the like): the mission ends, the plunger blocker back,
	// lights 504/505 as in normal play; the countdown text; with the light sweep done, sink 8 lets go and the mode
	// timer runs
	void PlainMissionEnd()
	{
		MissionEnd();
		Field(v_lite606)++;
		Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
		Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
	}

	void PlainMissionCommon(MessageCode code, TPinballComponent* cmp, int countdownText, float time)
	{
		switch (code)
		{
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			PlainMissionEnd();
			break;
		case FT1Event80:
		{
			auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
			if (left)
				Display(Format(countdownText, left / 1000).c_str(), 0.5f, 0);
			break;
		}
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
				Send(mode_timer, MessageCode::Ft1BlockerTimer, time);
				MissionEvent(FT1Event80, nullptr);
			}
			break;
		default:
			break;
		}
	}

	// 23:c5cc - state 9 (text 3064, tabb6, tabb73): 100,000, the light sweep 10, blockers c, f and g at 0;
	// 48.28 s (countdown text 3051)
	void State9(MessageCode code, TPinballComponent* cmp)
	{
		if (code != FT1Event79)
		{
			PlainMissionCommon(code, cmp, 3051, 48.28f);
			return;
		}
		MissionStart();
		ScorePoints(nullptr, 100000);
		Send(light_sweeps, MessageCode::Ft1LightSweepStart, 10.0f);
		PlayMusic(6); // g_22e2
		Play(tabb73wave);
		for (auto blocker : {&a_blocc, &a_blocf, &a_blocg})
			Send(*blocker, MessageCode::Ft1BlockerSetPosition, 0.0f);
		Display(ft1_strings::Get(3064), -1.0f, 0);
	}

	// 23:c84b - state 10 (text 3065, tabb2, tabb75): 250,000, the light sweep 11, lights 9-11 and 548-551 flash,
	// drops 10-12 up; 29.38 s (countdown text 3052)
	void State10(MessageCode code, TPinballComponent* cmp)
	{
		if (code != FT1Event79)
		{
			PlainMissionCommon(code, cmp, 3052, 29.38f);
			return;
		}
		MissionStart();
		ScorePoints(nullptr, 250000);
		Send(light_sweeps, MessageCode::Ft1LightSweepStart, 11.0f);
		Lights({&v_lite9, &v_lite10, &v_lite11, &v_lite551, &v_lite550, &v_lite549, &v_lite548},
		       MessageCode::TLightFlasherStartTimed, -1.0f);
		Drops10Up();
		PlayMusic(2); // g_22d2
		Play(tabb75wave);
		Display(ft1_strings::Get(3065), -1.0f, 0);
	}
	// 23:cb43 - the treasure mission's table: the multiball lights out, sinks 9 and 12 on, tripwires 3 and 8 off,
	// v_lite568's MessageField keeps whether it was lit, the orbit lights' overrides off, lights 4-8, 51, 567 and
	// 568 flash, the light sweep 4, lights 520/521 off
	void TreasureSetup()
	{
		Helper211c();
		for (auto sink : {&a_sink9, &a_sink12})
			if (auto comp = sink->GetComponent())
				comp->ActiveFlag = 1;
		for (auto trip : {&s_trip3, &s_trip8})
			if (auto comp = trip->GetComponent())
				comp->ActiveFlag = 0;
		Field(v_lite568) = LightOn(v_lite568) ? 1 : 0;
		Send(left_orbit_light_group, FT1LightOverrideOff, 0.0f);
		Send(right_orbit_light_group, FT1LightOverrideOff, 0.0f);
		Lights({&v_mlite51, &v_lite4, &v_lite5, &v_lite6, &v_lite7, &v_lite8, &v_lite568, &v_lite567},
		       MessageCode::TLightFlasherStartTimed, -1.0f);
		Send(light_sweeps, MessageCode::Ft1LightSweepStart, 4.0f);
		Send(v_lite520, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(v_lite521, MessageCode::TLightResetAndTurnOff, 0.0f);
	}

	// 23:0d35 - on to state 12 (the multiball) with the skill light lit
	void StartMultiball()
	{
		SkillLightOn();
		SetInfoField(12);
		MissionEvent(FT1Event79, nullptr);
	}

	// 23:cd08 - state 11, the treasure (text 3066, tabb3, tabb99): 1,000,000, the treasure table; light rollovers add
	// 50,000 to the jackpot; 30.68 s (countdown text 3054). When a rule moves g_651c on: the timer stops, tabb28,
	// the pictures "L" to "S" a quarter second each, then the tally (coins and special treasures 500,000 each, map
	// pieces and v_lite606's count 1,000,000 each, the fathoms, bumpers and jackpot; text 3003 4 s), then the
	// mission ends, sink 8 lets go and the multiball starts. Its end: the table as it was, g_22f6 set
	void State11(MessageCode code, TPinballComponent* cmp)
	{
		switch (code)
		{
		case FT1Event79:
			MissionStart();
			ScorePoints(nullptr, 1000000);
			TreasureSetup();
			PlayMusic(3); // g_22d6
			Play(tabb99wave);
			Display(ft1_strings::Get(3066), -1.0f, 0);
			break;
		case FT1Event83:
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			MissionEvent(FT1Event82, nullptr);
			break;
		case FT1Event82:
			Helper211c();
			Helper21be();
			PlainMissionEnd();
			for (auto trip : {&s_trip3, &s_trip8})
				if (auto comp = trip->GetComponent())
					comp->ActiveFlag = 1;
			Send(left_orbit_light_group, FT1LightResetOverride, 0.0f);
			Send(right_orbit_light_group, FT1LightResetOverride, 0.0f);
			Send(left_orbit_combo_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(right_orbit_combo_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite568, Field(v_lite568) ? MessageCode::TLightResetAndTurnOn : MessageCode::TLightResetAndTurnOff,
			     0.0f);
			Send(v_lite567, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(map_piece_light_group, MessageCode::TLightTurnOff, 0.0f);
			Send(map_piece_light_group, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Lights({&v_lite552, &v_lite553, &v_lite554, &v_lite555, &v_lite556, &v_lite557},
			       MessageCode::TLightResetAndTurnOff, 0.0f);
			g_22f6 = 1;
			break;
		case FT1Event80:
			switch (g_651c)
			{
			case 0:
			{
				auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
				if (left)
					Display(Format(3054, left / 1000).c_str(), 0.5f, 0);
				break;
			}
			case 1:
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
				Play(tabb28wave);
				[[fallthrough]];
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			{
				static const char* letters[8] = {"L", "M", "N", "O", "P", "Q", "R", "S"}; // ds:0x226a + n * 2
				Display(letters[g_651c - 1], 0.25f, 1);
				g_651c++;
				break;
			}
			case 9:
			{
				int total = Send(coin_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) * 500000;
				total += Send(special_treasure_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) * 500000;
				total += Send(map_piece_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) * 1000000;
				total += Field(v_lite606) * 1000000;
				FathomAward(-1.0f);
				BumperAward(-1.0f);
				CollectJackpot(0.0f);
				auto scored = ScorePoints(nullptr, total);
				Display(Format(3003, scored).c_str(), 4.0f, 0);
				g_651c++;
				break;
			}
			default:
				MissionEvent(FT1Event82, nullptr);
				ReleaseSink8(0.0f);
				StartMultiball();
				break;
			}
			break;
		case MessageCode::ControlCollision:
			if (dynamic_cast<TLightRollover*>(cmp)) // 1020
				AddJackpot(50000);
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
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.68f);
				MissionEvent(FT1Event80, nullptr);
			}
			break;
		default:
			break;
		}
	}

	// 23:d2b8 - state 12, the multiball (tabb32, text 3055): kicker 1 on, oneway 12 off, sink 8 lets the balls kept
	// there go (two more balls in play); when only one is left: the mission ends, kicker 1 off, lights 548-551
	// flash out, back to normal play
	void State12(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		switch (code)
		{
		case FT1Event79:
			MissionStart();
			Send(a_kick1, MessageCode::Ft1Activate, 0.0f); // FT1 75
			if (auto oneway = s_oneway12.GetComponent())
				oneway->ActiveFlag = 0;
			switch (g_6526)
			{
			case 0:
				ReleaseSink8(2.0f);
				[[fallthrough]];
			case 1:
				ReleaseSink8(1.0f);
				break;
			default:
				break;
			}
			t->MultiballCount += 2; // +0x146
			g_6526 = 0;
			InfoClear();
			Play(tabb32wave);
			[[fallthrough]];
		case FT1Event80:
			Display(ft1_strings::Get(3055), -1.0f, 0);
			break;
		case FT1Event83:
			if (t->MultiballCount != 1)
				break;
			MissionEnd();
			InfoClear();
			Send(a_kick1, MessageCode::Ft1Deactivate, 0.0f); // FT1 76
			SetInfoField(2);
			Lights548(1);
			MissionEvent(FT1Event79, nullptr);
			break;
		default:
			break;
		}
	}
	// 23:d414 - state 13, the kept balls' multiball (tabb95, text 3056): with no balls kept (g_6526) lights 530-532
	// flash out and back to normal play; else they go back in play, lights 530-532 flash out, kicker 1 on, oneway
	// 12 off, 100,000; when only one ball is left: the mission ends, kicker 1 off, back to normal play
	void State13(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		switch (code)
		{
		case FT1Event79:
			if (!g_6526)
			{
				Lights530(true);
				SetInfoField(2);
				MissionEvent(FT1Event79, nullptr);
				return;
			}
			MissionStart();
			Field(v_lite551) = 0xf; // as the original has it (Bits548 reads a_spot11's)
			Bits548();
			t->MultiballCount += g_6526; // +0x146
			g_6526 = 0;
			Lights({&v_lite530, &v_lite531, &v_lite532}, MessageCode::TLightTurnOn, 0.0f);
			Lights({&v_lite530, &v_lite531, &v_lite532}, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Send(a_kick1, MessageCode::Ft1Activate, 0.0f); // FT1 75
			if (auto oneway = s_oneway12.GetComponent())
				oneway->ActiveFlag = 0;
			ScorePoints(nullptr, 100000);
			InfoClear();
			Play(tabb95wave);
			[[fallthrough]];
		case FT1Event80:
			Display(ft1_strings::Get(3056), -1.0f, 0);
			break;
		case FT1Event83:
			if (t->MultiballCount != 1)
				break;
			MissionEnd();
			InfoClear();
			Send(a_kick1, MessageCode::Ft1Deactivate, 0.0f); // FT1 76
			SetInfoField(2);
			MissionEvent(FT1Event79, nullptr);
			Lights530(false);
			Lights548(1);
			break;
		default:
			break;
		}
	}

	int g_6540 = 0; // the bonus count is hurried (a flipper)
	int g_6542 = 0; // the bonus count's step
	int g_6544 = 0; // the bonus so far

	// 23:b27e - state 16, the end of the ball: the bonus is counted, one line per award time (a flipper hurries it;
	// nothing when tilted): popup targets x10,000 (3028), v_lite606's count x200,000 (3029), v_lite607's x250,000
	// (3030), special treasures x100,000 (3031), the held bonus (3032); the total is scored (3033, tabb74) and kept
	// when v_lite570 (bonus hold) is lit. Then an extra ball, the next player (on to the skill shot, state 3), or the
	// game is over (state 1). mode_timer (30 s) ends it if it hangs.
	void State16(MessageCode code, TPinballComponent* cmp)
	{
		auto t = control::TableG;
		switch (code)
		{
		case MessageCode::TFlipperExtend: // FT1 1
			g_6540 = 1;
			InfoClear();
			State16(FT1Event80, cmp);
			return;
		case FT1Event83:
			if (g_6542 >= 6)
				break;
			return;
		case MessageCode::ControlTimerExpired: // FT1 70
			if (cmp != mode_timer.GetComponent())
				return;
			g_22f2 = 0;
			g_6542 = 8;
			State16(FT1Event80, nullptr);
			return;
		case FT1Event79:
			g_6540 = 0;
			g_6542 = 0;
			g_6544 = 0;
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.0f);
			if (t->TiltLockFlag && g_22f2 != 1) // +0x15a
				g_6542 = 6;
			InfoClear();
			Play(tabb42wave);
			break;
		case FT1Event80:
			break;
		default:
			return;
		}
		// event 80: the next step
		for (;;)
		{
			auto step = g_6542++;
			int value = 0, text = 0;
			switch (step)
			{
			case 0:
				value = g_6524 * 10000;
				text = 3028;
				break;
			case 1:
				value = Field(v_lite606) * 200000;
				text = 3029;
				break;
			case 2:
				value = Field(v_lite607) * 250000;
				text = 3030;
				break;
			case 3:
				value = Field(special_treasure_light_group) * 100000;
				text = 3031;
				break;
			case 4:
				value = Field(drain); // the held bonus
				text = 3032;
				break;
			case 5:
				if (LightOn(v_lite570))
				{
					Send(v_lite570, MessageCode::TLightResetAndTurnOff, 0.0f);
					Field(drain) = g_6544;
				}
				else
					Field(drain) = 0;
				if (!g_6544)
					continue;
				{
					auto added = SpecialAddScore(cmp, g_6544);
					if (g_6540)
						continue;
					Play(tabb74wave);
					Display(Format(3033, added).c_str(), Step() + Step() / 2.0f, 0);
				}
				return;
			default:
			{
				// the ball is over
				if (g_22f2 && t->MultiballCount)
					return;
				g_22f2 = 0;
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
				if (t->ExtraBalls) // +0x144
				{
					t->ExtraBalls--;
					auto player = t->CurrentPlayer;
					Display(ft1_strings::Get(player >= 0 && player <= 2 ? 2011 + player : 2014), -1.0f, 0);
					t->Message(MessageCode::ClearTiltLock, 0.0f); // FT1 1012
				}
				else
				{
					t->ChangeBallCount(t->BallCount - 1); // 15:b9c6
					if (t->CurrentPlayer + 1 == t->PlayerCount && t->BallCount == 0)
					{
						if (t->LightGroup)
							t->LightGroup->Message(MessageCode::TLightResetAndTurnOff, 0.0f);
						SetInfoField(1);
						MissionEvent(FT1Event79, nullptr);
						if (t->LightGroup)
							t->LightGroup->Message(FT1LightResetOverride, 0.0f);
						return;
					}
					t->Message(MessageCode::ClearTiltLock, 0.0f);    // FT1 1012
					Send(drain, MessageCode::Reset, 0.0f);             // FT1 1011 to the drain
					t->Message(MessageCode::SwitchToNextPlayer, 0.0f); // FT1 1023
				}
				Send(s_launcher1, MessageCode::PlungerStartFeedTimer, 0.96f); // FT1 1017
				SetInfoField(3);
				MissionEvent(FT1Event79, nullptr);
				if (t->LightGroup)
					t->LightGroup->Message(FT1LightResetOverride, 0.0f);
				return;
			}
			}
			if (!value)
				continue;
			g_6544 += value;
			if (g_6540)
				continue;
			Play(tabb84wave);
			Display(Format(text, value).c_str(), Step(), 0);
			return;
		}
	}
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
	// 23:3c73 - the map mission picked (mode_timer's MessageField 1-8) starts, by the skill shot's helpers
	void StartPicked()
	{
		switch (Field(mode_timer))
		{
		case 1: SkillMission(4, "H", 1.0f); break;   // 23:0b38
		case 2: SkillMission(8, "I", 2.0f); break;   // 23:0c4a
		case 3: SkillMission(7, "333", 2.0f); break; // 23:0bff
		case 4: SkillMission(5, "J", 2.0f); break;   // 23:0b83
		case 5: SkillMission(6, nullptr, 0.0f); break; // 23:0bce
		case 6: SkillMission(9, nullptr, 0.0f); break; // 23:0c95
		case 7: // 23:0cc6
			Field(a_spot8) = 0;
			SkillMission(10, nullptr, 0.0f);
			break;
		case 8: SkillMission(11, nullptr, 0.0f); break; // 23:0d04
		default:
			break;
		}
	}

	bool AllDropsDown() { return (Field(v_lite1) & 0x7ff) == 0x7ff; } // v_lite1's MessageField: all 11

	// 23:40fc - all 11 drop targets down (in the multiball): their lights flash out, all banks up, a map piece
	bool AllDropsMultiball()
	{
		if (!AllDropsDown())
			return false;
		Lights1to11(true);
		Drops13Up();
		Drops5Up();
		Drops10Up();
		MapPieceAward(0.0f);
		return true;
	}

	// 23:40a9 - all 11 drop targets down (normal play): sink 8 keeps its ball (g_6520), their lights flash out, all
	// banks up, the picked map mission starts
	bool AllDropsNormal()
	{
		if (!AllDropsDown())
			return false;
		g_6520 = 1;
		Lights1to11(true);
		Drops13Up();
		Drops5Up();
		Drops10Up();
		StartPicked();
		return true;
	}

	// 23:2b32 - a magnet: one of the magnets whose light (560-565) is off at random (all off: any) is switched on
	// with its light; the keg picture ("E") 2 s later
	float MagnetAward(float time)
	{
		component_tag_base* magnets[6] = {&s_magnet1, &s_magnet2, &s_magnet3, &s_magnet4, &s_magnet5, &s_magnet6};
		component_tag<TLight>* lights[6] = {&v_lite560, &v_lite561, &v_lite562, &v_lite563, &v_lite564, &v_lite565};
		int candidates[6], count = 0;
		for (int i = 0; i < 6; i++)
			if (!LightOn(*lights[i]))
				candidates[count++] = i;
		if (!count)
			for (int i = 0; i < 6; i++)
				candidates[count++] = i;
		auto n = candidates[Rand15() * count / 0x8000];
		Send(*magnets[n], MessageCode::Ft1Activate, 0.0f); // FT1 75
		Send(*lights[n], MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		Send(keg_light_group, MessageCode::TLightGroupGetOnCount, 0.0f); // (the result unused)
		time += 2.0f;
		Display("E", time, 1); // ds:0x2260
		return time;
	}

	// 23:34bf - the next of a treasure, a coin, a map piece, then extra balls (g_651e)
	float NextAward(float time)
	{
		switch (g_651e)
		{
		case 0: time = TreasureAward(time); break;
		case 1: time = AwardCoin(time); break;
		case 2: time = MapPieceAward(time); break;
		default: time = ExtraBallAward(time); break;
		}
		g_651e++;
		return time;
	}

	// 23:362d - state 10: drops 10-12 (bits 4-6 of g_651c) and the spots (bits 0-3) both done: the next award
	float State10Award(float time)
	{
		if ((g_651c & 0x70) == 0x70)
			g_651c &= 0xf;
		else if ((g_651c & 0xf) == 0xf)
			g_651c &= 0x70;
		else
			return time;
		return NextAward(time);
	}

	// The plunger blockers (and lights 504/505) for a ball in the drop target banks' award
	void BankBlockers(float blocc)
	{
		Send(a_blocc, MessageCode::Ft1BlockerSetPosition, blocc);
		Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 1.0f);
		Send(v_lite504, MessageCode::TLightResetAndTurnOn, 0.0f);
		Send(v_lite505, MessageCode::TLightResetAndTurnOff, 0.0f);
	}

	// 23:48e3 - drop targets 5-9 (a_drop5's MessageField, 5 bits). State 6: all down, up again. State 4: all down,
	// up again and the jackpot (the fathoms' timer restarted), 25,000; else 10,000 (tabb22). All 11 down in normal
	// play or the multiball: their award. Else all five down: blockers c 0, f away, g 0, up again, the bank's second
	// score (tabb24); a target: its first score (tabb22)
	void Bank5(TPinballComponent* caller)
	{
		bool all = (Field(a_drop5) & 0x1f) == 0x1f;
		switch (InfoField())
		{
		case 6:
			if (all)
				Drops5Up();
			return;
		case 4:
			if (all)
			{
				Drops5Up();
				auto time = CollectJackpot(0.0f);
				Send(fathom_light_group, MessageCode::TLightGroupRestartNotifyTimer, time); // FT1 47
				ScorePoints(caller, 25000);
			}
			else
			{
				Play(tabb22wave);
				ScorePoints(caller, 10000);
			}
			return;
		case 12:
			if (AllDropsMultiball())
				return;
			break;
		case 2:
			if (AllDropsNormal())
				return;
			break;
		default:
			break;
		}
		if (!all)
		{
			ScoreHit(caller, 0);
			Play(tabb22wave);
			return;
		}
		Send(a_blocc, MessageCode::Ft1BlockerSetPosition, 0.0f);
		Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 1.0f);
		Send(v_lite504, MessageCode::TLightResetAndTurnOn, 0.0f);
		Send(v_lite505, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(a_blocf, MessageCode::Ft1BlockerSetPosition, -1.0f);
		Send(a_blocg, MessageCode::Ft1BlockerSetPosition, 0.0f);
		Drops5Up();
		ScoreHit(caller, 1);
		Play(tabb24wave);
	}

	// 23:4dbf - drop targets 13-15 (a_drop13's, 3 bits). State 6: all down, up again. State 7: all down, the blockers
	// back, up again, a magnet and the jackpot, 50,000; else a magnet, 25,000 (tabb87). All 11 down in normal play or
	// the multiball: their award. Else all three: the blockers back, up again, the second score (tabb29); a target:
	// its first score (tabb02)
	void Bank13(TPinballComponent* caller)
	{
		bool all = (Field(a_drop13) & 7) == 7;
		auto blockers = [] {
			BankBlockers(0.0f);
			Send(a_blocf, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(a_blocg, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Drops13Up();
		};
		switch (InfoField())
		{
		case 6:
			if (all)
				Drops13Up();
			return;
		case 7:
			if (all)
			{
				blockers();
				CollectJackpot(MagnetAward(0.0f));
				ScorePoints(nullptr, 50000);
			}
			else
			{
				MagnetAward(0.0f);
				Play(tabb87wave);
				ScorePoints(nullptr, 25000);
			}
			return;
		case 12:
			if (AllDropsMultiball())
				return;
			break;
		case 2:
			if (AllDropsNormal())
				return;
			break;
		default:
			break;
		}
		if (!all)
		{
			ScoreHit(caller, 0);
			Play(tabb02wave);
			return;
		}
		blockers();
		ScoreHit(caller, 1);
		Play(tabb29wave);
	}

	// 23:5201 - drop targets 10-12 (a_drop10's, 3 bits). State 6: all down, up again. State 10: all down, their
	// lights on (the first time, bits 4-6 of g_651c) or flashing on, up again, maybe the next award; else 30,000;
	// tabb94. State 7: all down, up again, the second score (tabb19). All 11 down in normal play or the multiball:
	// their award. Else all three: blockers c away, e up, f and g 0, up again, the second score (tabb19); a target:
	// its first score (tabb07)
	void Bank10(TPinballComponent* caller)
	{
		bool all = (Field(a_drop10) & 7) == 7;
		auto second = [caller] {
			Drops10Up();
			ScoreHit(caller, 1);
			Play(tabb19wave);
		};
		switch (InfoField())
		{
		case 6:
			if (all)
				Drops10Up();
			return;
		case 10:
			if (all)
			{
				if (!(g_651c & 0x70))
				{
					g_651c |= 0x70;
					Lights({&v_lite9, &v_lite10, &v_lite11}, MessageCode::TLightResetAndTurnOn, 0.0f);
				}
				else
					Lights({&v_lite9, &v_lite10, &v_lite11}, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
				Drops10Up();
				State10Award(0.0f);
			}
			else
				ScorePoints(nullptr, 30000);
			Play(tabb94wave);
			return;
		case 7:
			if (all)
				second();
			else
			{
				Play(tabb07wave);
				ScoreHit(caller, 0);
			}
			return;
		case 12:
			if (AllDropsMultiball())
				return;
			break;
		case 2:
			if (AllDropsNormal())
				return;
			break;
		default:
			break;
		}
		if (!all)
		{
			Play(tabb07wave);
			ScoreHit(caller, 0);
			return;
		}
		BankBlockers(-1.0f);
		Send(a_blocf, MessageCode::Ft1BlockerSetPosition, 0.0f);
		Send(a_blocg, MessageCode::Ft1BlockerSetPosition, 0.0f);
		second();
	}

	// 23:4b23 ... 23:55c8 - a drop target: its bit in its bank's MessageField; in normal play and the multiball also
	// in v_lite1's (all 11) with its light flashing on; then its bank's rule
	void DropRule(MessageCode code, TPinballComponent* caller, component_tag_base& bank, int bit, int allBit,
	              component_tag<TLight>& light, void (*bankRule)(TPinballComponent*))
	{
		if (code != MessageCode::ControlCollision)
			return;
		Field(bank) |= bit;
		if (InfoField() == 2 || InfoField() == 12)
		{
			Field(v_lite1) |= allBit;
			Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		}
		bankRule(caller);
	}

	void Rule_4b23(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop5, 1, 0x8, v_lite4, Bank5); }
	void Rule_4ba8(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop5, 2, 0x10, v_lite5, Bank5); }
	void Rule_4c2d(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop5, 4, 0x20, v_lite6, Bank5); }
	void Rule_4cb2(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop5, 8, 0x40, v_lite7, Bank5); }
	void Rule_4d37(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop5, 0x10, 0x80, v_lite8, Bank5); }
	void Rule_5076(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop13, 1, 1, v_lite1, Bank13); }
	void Rule_50f7(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop13, 2, 2, v_lite2, Bank13); }
	void Rule_517c(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop13, 4, 4, v_lite3, Bank13); }
	void Rule_54b8(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop10, 1, 0x100, v_lite9, Bank10); }
	void Rule_5540(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop10, 2, 0x200, v_lite10, Bank10); }
	void Rule_55c8(MessageCode c, TPinballComponent* p) { DropRule(c, p, a_drop10, 4, 0x400, v_lite11, Bank10); }

	// 23:5650 / 23:56af - the rebounders: their light on 0.1 s, their score
	void Rule_5650(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Send(v_lite84, MessageCode::TLightTurnOnTimed, 0.1f);
		ScoreHit(caller, 0);
	}

	void Rule_56af(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Send(v_lite85, MessageCode::TLightTurnOnTimed, 0.1f);
		ScoreHit(caller, 0);
	}

	// 23:570e - a_spot102: in normal play the next map mission is picked (its text 3068-3075, 2 s); in a map
	// mission (4-11) 1/1000 of its time left plus 10 s more on the clock; v_lite607 flashes 1 s, the first score
	void Rule_570e(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		auto state = InfoField();
		if (state == 2)
		{
			auto n = PickNext();
			Display(ft1_strings::Get(n >= 0 && n <= 6 ? 3068 + n : 3075), 2.0f, 0);
			ScoreHit(caller, 0);
			Send(v_lite607, MessageCode::TLightFlasherStartTimed, 1.0f);
			return;
		}
		if (state >= 4 && state <= 11)
		{
			auto left = Send(mode_timer, MessageCode::Ft1TimerTimeLeft, 0.0f);
			if (left > 0)
				Send(mode_timer, MessageCode::Ft1BlockerTimer, static_cast<float>(left) / 1000.0f + 10.0f);
		}
		Send(v_lite607, MessageCode::TLightFlasherStartTimed, 1.0f);
		ScoreHit(caller, 0);
	}

	// 23:59a2 - spots 11, 8, 9, 10 all lit (a_spot11's MessageField): cleared, the second score, the multiball;
	// else the first score
	void Spots(TPinballComponent* caller)
	{
		if ((Field(a_spot11) & 0xf) == 0xf)
		{
			Field(a_spot11) = 0;
			ScoreHit(caller, 1);
			StartMultiball();
		}
		else
			ScoreHit(caller, 0);
	}

	// 23:351b - state 10's spots: v_lite550's MessageField all four (as the original has it): cleared, lights
	// 548-551 flash; else 30,000
	float State10Spots(float time)
	{
		if ((Field(v_lite550) & 0xf) == 0xf)
		{
			Field(v_lite550) = 0;
			Lights({&v_lite551, &v_lite550, &v_lite549, &v_lite548}, MessageCode::TLightResetTimed, 0.0f);
			Lights({&v_lite551, &v_lite550, &v_lite549, &v_lite548}, MessageCode::TLightFlasherStartTimed, 1.0f);
		}
		else
			ScorePoints(nullptr, 30000);
		return time;
	}

	// 23:59f4 / 5b27 / 5c6a / 5d9d - spots 11, 8, 9, 10 (bits 1, 2, 4, 8; lights 551-548): in normal play their bit
	// in a_spot11's MessageField and their light, then all four checked (tabb23); in state 10 their bit in g_651c,
	// maybe the next award (tabb94); else tabb23 and the first score
	void SpotRule(MessageCode code, TPinballComponent* caller, int bit, component_tag<TLight>& light, bool soundFirst)
	{
		if (code != MessageCode::ControlCollision)
			return;
		switch (InfoField())
		{
		case 2:
			if (soundFirst)
				Play(tabb23wave);
			Field(a_spot11) |= bit;
			Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Spots(caller);
			if (!soundFirst)
				Play(tabb23wave);
			break;
		case 10:
			g_651c |= bit;
			Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			if (bit == 8)
				Field(a_spot8) |= 8; // as the original has it
			State10Award(State10Spots(0.0f));
			Play(tabb94wave);
			break;
		default:
			Play(tabb23wave);
			ScoreHit(caller, 0);
			break;
		}
	}

	void Rule_59f4(MessageCode c, TPinballComponent* p) { SpotRule(c, p, 1, v_lite551, false); }
	void Rule_5b27(MessageCode c, TPinballComponent* p) { SpotRule(c, p, 2, v_lite550, true); }
	void Rule_5c6a(MessageCode c, TPinballComponent* p) { SpotRule(c, p, 4, v_lite549, false); }
	void Rule_5d9d(MessageCode c, TPinballComponent* p) { SpotRule(c, p, 8, v_lite548, false); }

	// 23:5eda - spots 4 and 5 both lit (v_lite522's MessageField): blocker f away, cleared, their lights flash out,
	// the second score; else the first; tabb02
	void Spots45(TPinballComponent* caller)
	{
		if ((Field(v_lite522) & 3) == 3)
		{
			Send(a_blocf, MessageCode::Ft1BlockerSetPosition, -1.0f);
			Field(v_lite522) = 0;
			Send(v_lite522, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Send(v_lite779, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			ScoreHit(caller, 1);
		}
		else
			ScoreHit(caller, 0);
		Play(tabb02wave);
	}

	// 23:5f7f / 5fe2 - spots 4 and 5: their bit and light (v_lite522 / v_lite779), then both checked
	void Rule_5f7f(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Field(v_lite522) |= 1;
		Send(v_lite522, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		Spots45(caller);
	}

	void Rule_5fe2(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Field(v_lite522) |= 2;
		Send(v_lite779, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		Spots45(caller);
	}

	// 23:6049 - a_spot6, three hits (its MessageField, lights 530-532): in state 7 the third lights them all
	// flashing out and a bumper award (tabb61); in normal play the third starts state 13 (the kept balls'
	// multiball); each other hit tabb02 and the first score
	void Rule_6049(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		auto state = InfoField();
		if (state != 7 && state != 2)
			return;
		auto& hits = Field(a_spot6);
		if (hits == 0 || hits == 1)
		{
			Play(tabb02wave);
			hits++;
			ScoreHit(caller, 0);
			if (state == 7)
			{
				if (hits == 1)
				{
					Send(v_lite530, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					Send(v_lite531, MessageCode::TLightResetAndTurnOff, 0.0f);
				}
				else
				{
					Send(v_lite530, MessageCode::TLightResetAndTurnOn, 0.0f);
					Send(v_lite531, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
				}
				Send(v_lite532, MessageCode::TLightResetAndTurnOff, 0.0f);
			}
			else
				Send(hits == 1 ? v_lite530 : v_lite531, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			return;
		}
		if (state == 7)
		{
			Play(tabb61wave);
			Lights530(true);
			BumperAward(0.0f);
			ScoreHit(caller, 0);
		}
		else
		{
			hits = 0;
			ScoreHit(caller, 0);
			SkillMission(13, nullptr, 0.0f); // 23:0d66
		}
	}

	// 23:0df8 - the ball leaves the sink: in normal play from sink 8, else from sink 17, 15 or 14 at random
	void SinkExit()
	{
		if (InfoField() == 2)
		{
			ReleaseSink8(-1.0f);
			return;
		}
		component_tag_base* sinks[3] = {&a_sink17, &a_sink15, &a_sink14};
		auto n = Rand15() * 3 / 0x8000;
		Send(*sinks[n > 2 ? 2 : n], MessageCode::TSinkResetTimer, -1.0f); // FT1 65
	}

	// 23:1188 - the four lane sinks' lights (v_lite524's MessageField) all lit: they flash out, a coin
	float LaneSinks(float time)
	{
		if ((Field(v_lite524) & 0xf) == 0xf)
		{
			Lights524(true);
			time = AwardCoin(time);
		}
		return time;
	}

	// 23:2089 - drop targets 5-9 all down (a_drop5's MessageField all five)
	void Drops5Down()
	{
		Field(a_drop5) = 0x1f;
		for (auto drop : {&a_drop5, &a_drop6, &a_drop7, &a_drop8, &a_drop9})
			Send(*drop, MessageCode::TPopupTargetDisable, 0.0f); // FT1 53
	}

	// The first score of a sink with its text (3002, 2 s)
	void SinkScore(TPinballComponent* caller)
	{
		Display(Format(3002, ScoreHit(caller, 0)).c_str(), 2.0f, 0);
	}

	// 23:62f4 / 65a6 / 6873 - sinks 17, 15, 14 (multiball light v_mlite44 / 107 / 106, lane light v_lite524 / 525 /
	// 526 with bit 1 / 2 / 4, their light v_lite519 / 520 / 521). State 11: the next treasure. State 8: the
	// multiball shot flashing there: done, its lights, the jackpot (sink 17) or the next treasure, the next shot,
	// sink 8 lets go; else the score. State 6: tabb11. State 5: the score, tabb70, the award (1248). Normal play:
	// the score, the lane light and its bit, maybe a coin. The ball leaves (SinkExit) except for a multiball shot;
	// their light flashes 2 s
	void LaneSinkRule(MessageCode code, TPinballComponent* caller, component_tag<TLight>& multi,
	                  component_tag<TLight>& lane, int bit, component_tag<TLight>& light, bool jackpot, bool lightFirst)
	{
		if (code != MessageCode::ControlCollision)
			return;
		switch (InfoField())
		{
		case 11:
			TreasureAward(0.0f);
			SinkExit();
			break;
		case 8:
			if (multi.Component && multi.Component->FlasherOnFlag) // +0x3c
			{
				Send(multi, MessageCode::TLightResetAndTurnOn, 0.0f);
				Send(lane, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(light, MessageCode::TLightResetAndTurnOff, 0.0f);
				if (jackpot)
					CollectJackpot(0.0f);
				else
					TreasureAward(0.0f);
				NextMultiShot();
				ReleaseSink8(-1.0f);
			}
			else
			{
				SinkScore(caller);
				Play(tabb11wave);
				SinkExit();
			}
			break;
		case 6:
			SinkExit();
			Play(tabb11wave);
			break;
		case 5:
			ScoreHit(caller, 0);
			Play(tabb70wave);
			Award1248(0.0f);
			SinkExit();
			break;
		case 2:
			if (lightFirst)
				Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			SinkScore(caller);
			Send(lane, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Field(v_lite524) |= bit;
			LaneSinks(2.0f);
			Play(tabb11wave);
			SinkExit();
			break;
		default:
			SinkScore(caller);
			Play(tabb11wave);
			SinkExit();
			break;
		}
		Send(light, MessageCode::TLightFlasherStartTimed, 2.0f);
	}

	void Rule_62f4(MessageCode c, TPinballComponent* p)
	{
		LaneSinkRule(c, p, v_mlite44, v_lite524, 1, v_lite519, true, false);
	}
	void Rule_65a6(MessageCode c, TPinballComponent* p)
	{
		LaneSinkRule(c, p, v_mlite107, v_lite525, 2, v_lite520, false, true);
	}
	void Rule_6873(MessageCode c, TPinballComponent* p)
	{
		LaneSinkRule(c, p, v_mlite106, v_lite526, 4, v_lite521, false, true);
	}

	// 23:6b40 - a_sink4 (the treasure chest). State 11: the treasure's tally starts (g_651c 1), its lights out.
	// State 9: tabb27, v_lite570 on, v_lite567 flashes, sink 8 lets go. State 4: drops 5-9 down, the jackpot (the
	// fathoms' timer restarted), sink 8 lets go. Else: blocker f at 0, v_lite567 flashes on, blockers c, f, g away,
	// sink 8 lets go; in normal play with v_lite568 lit an extra ball, else the score (text 3003); tabb27
	void Rule_6b40(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		switch (InfoField())
		{
		case 11:
			Send(v_mlite51, MessageCode::TLightResetAndTurnOn, 0.0f);
			Lights({&v_lite4, &v_lite5, &v_lite6, &v_lite7, &v_lite8, &v_lite568, &v_lite567},
			       MessageCode::TLightResetAndTurnOff, 0.0f);
			g_651c = 1;
			return;
		case 9:
			Play(tabb27wave);
			Send(v_lite570, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(v_lite567, MessageCode::TLightFlasherStartTimed, 1.0f);
			ReleaseSink8(-1.0f);
			return;
		case 4:
		{
			Drops5Down();
			g_651c++;
			auto time = CollectJackpot(0.0f);
			Send(fathom_light_group, MessageCode::TLightGroupRestartNotifyTimer, time); // FT1 47
			ReleaseSink8(-1.0f);
			return;
		}
		default:
			break;
		}
		Send(a_blocf, MessageCode::Ft1BlockerSetPosition, 0.0f);
		Send(v_lite567, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		for (auto blocker : {&a_blocc, &a_blocf, &a_blocg})
			Send(*blocker, MessageCode::Ft1BlockerSetPosition, -1.0f);
		ReleaseSink8(-1.0f);
		if (InfoField() == 2 && LightOn(v_lite568))
		{
			Send(v_lite568, MessageCode::TLightResetAndTurnOff, 0.0f);
			ExtraBallAward(0.0f);
		}
		else
			Display(Format(3003, ScoreHit(caller, 0)).c_str(), 2.0f, 0);
		Play(tabb27wave);
	}

	// 23:6eef / 7061 - sinks 9 and 12 (the orbits' multiball shots). State 11: the next treasure. State 8: the shot
	// flashing there (v_mlite110 / 104): done, its orbit lights out, the next treasure, the next shot, sink 8 lets
	// go; else the score (3002), tabb11, the ball leaves. Else the sink goes off and the other one lets the ball go
	void OrbitSinkRule(MessageCode code, TPinballComponent* caller, component_tag<TLight>& multi,
	                   component_tag_base& orbit, component_tag_base& other)
	{
		if (code != MessageCode::ControlCollision)
			return;
		switch (InfoField())
		{
		case 11:
			TreasureAward(0.0f);
			SinkExit();
			break;
		case 8:
			if (multi.Component && multi.Component->FlasherOnFlag)
			{
				Send(multi, MessageCode::TLightResetAndTurnOn, 0.0f);
				Send(orbit, MessageCode::TLightResetAndTurnOff, 0.0f);
				TreasureAward(0.0f);
				NextMultiShot();
				ReleaseSink8(-1.0f);
			}
			else
			{
				SinkScore(caller);
				Play(tabb11wave);
				SinkExit();
			}
			break;
		default:
			if (caller)
				caller->ActiveFlag = 0;
			Send(other, MessageCode::TSinkResetTimer, 0.0f);
			break;
		}
	}

	void Rule_6eef(MessageCode c, TPinballComponent* p) { OrbitSinkRule(c, p, v_mlite110, left_orbit_light_group, a_sink12); }
	void Rule_7061(MessageCode c, TPinballComponent* p) { OrbitSinkRule(c, p, v_mlite104, right_orbit_light_group, a_sink9); }

	// 23:2f08 - a random award: a map piece, the next treasure, a coin or an extra ball
	float RandomAward2(float time)
	{
		switch (Rand15() * 4 / 0x8000)
		{
		case 0: return MapPieceAward(time);
		case 1: return TreasureAward(time);
		case 2: return AwardCoin(time);
		default: return ExtraBallAward(time);
		}
	}

	// 23:71d3 - the three cannon sinks' lights (v_lite547's MessageField) as a picture ("333" .. "444", 2 s); all
	// three: they flash out, cleared, the second score, else the first; tabb08, the score (text 3003, 4 s), the
	// blockers c, f, g away, sink 8 lets go
	void CannonSinks(TPinballComponent* caller)
	{
		static const char* pictures[8] = {"333", "334", "343", "344", "433", "434", "443", "444"}; // ds:0x2216
		Display(pictures[Field(v_lite547) & 7], 2.0f, 1);
		int points;
		if ((Field(v_lite547) & 7) == 7)
		{
			Lights({&v_lite547, &v_lite546, &v_lite545}, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Field(v_lite547) = 0;
			points = ScoreHit(caller, 1);
		}
		else
			points = ScoreHit(caller, 0);
		Play(tabb08wave);
		Display(Format(3003, points).c_str(), 4.0f, 0);
		for (auto blocker : {&a_blocc, &a_blocf, &a_blocg})
			Send(*blocker, MessageCode::Ft1BlockerSetPosition, -1.0f);
		ReleaseSink8(-1.0f);
	}

	// 23:734c / 742b / 750e - sinks 5, 6, 7 (lights 547, 546, 545; bits 1, 2, 4). State 7: the light blinking
	// there gives a random award, else tabb08; sink 8 lets go. Else their bit and light, then all three checked
	void CannonSinkRule(MessageCode code, TPinballComponent* caller, component_tag<TLight>& light, int bit)
	{
		if (code != MessageCode::ControlCollision)
			return;
		if (InfoField() == 7)
		{
			if (light.Component && light.Component->FlasherOnFlag) // +0x3c
				RandomAward2(0.0f);
			else
				Play(tabb08wave);
			ReleaseSink8(-1.0f);
			return;
		}
		Field(v_lite547) |= bit;
		Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		CannonSinks(caller);
	}

	void Rule_734c(MessageCode c, TPinballComponent* p) { CannonSinkRule(c, p, v_lite547, 1); }
	void Rule_742b(MessageCode c, TPinballComponent* p) { CannonSinkRule(c, p, v_lite546, 2); }
	void Rule_750e(MessageCode c, TPinballComponent* p) { CannonSinkRule(c, p, v_lite545, 4); }

	// 23:321d - state 5 (Bermuda), a ramp whose light flashes: the wind lights move on (g_651e); the next of
	// 125,000 / 250,000 / 500,000 / 1,000,000 (text 3003, tabb12), after the fourth the jackpot
	void BermudaShot()
	{
		static const int worth[4] = {125000, 250000, 500000, 1000000}; // ds:0x37cc
		WindLights(false);
		g_651e++;
		WindLights(true);
		if (g_651c < 4)
		{
			auto points = SpecialAddScore(nullptr, worth[std::max(0, g_651c)]);
			Display(Format(3003, points).c_str(), 2.0f, 0);
			Play(tabb12wave);
			g_651c++;
		}
		else
		{
			CollectJackpot(0.0f);
			g_651c = 0;
		}
	}

	// 23:75f1 / 78dd - the island ramps (a_ramp8: combo lights 629/630, island lights 603/604, text 3004; a_ramp5:
	// 626/627, the right wind lights 611/612, text 3005). Made (FT1 86): in state 5 the light flashing there is a
	// Bermuda shot, else 50,000 (tabb12); otherwise tabb12, the combo lights (0, 1, 2, more lit before: the score
	// 0, 3, 6, 9 up) and the island lights (in normal play; 0, 1, more: +0, +1, +2) light up 2 s, that score with
	// its text. Another shot scored (FT1 77): the combo lights out
	void IslandRamp(MessageCode code, TPinballComponent* caller, component_tag<TLightGroup>& combo,
	                component_tag<TLight>& combo1, component_tag<TLight>& combo2, component_tag<TLightGroup>& island,
	                component_tag<TLight>& island1, component_tag<TLight>& island2, int text)
	{
		if (code == LastHitMovedOn)
		{
			Send(combo, MessageCode::TLightResetAndTurnOff, 2.0f);
			return;
		}
		if (code != MessageCode::Ft1RampExit3)
			return;
		if (InfoField() == 5)
		{
			if (island1.Component && island1.Component->FlasherOnFlag)
			{
				BermudaShot();
				return;
			}
			SpecialAddScore(caller, 50000);
			Play(tabb12wave);
			return;
		}
		Play(tabb12wave);
		auto flash = [](component_tag_base& part) {
			Send(part, MessageCode::TLightFlasherStartTimedThenStayOn, 2.0f);
		};
		int index;
		switch (Send(combo, MessageCode::TLightGroupGetOnCount, 0.0f))
		{
		case 0:
			flash(combo1);
			index = 0;
			break;
		case 1:
			flash(combo1);
			flash(combo2);
			index = 3;
			break;
		case 2:
			flash(combo);
			index = 6;
			break;
		default:
			flash(combo);
			index = 9;
			break;
		}
		bool normal = InfoField() == 2;
		switch (Send(island, MessageCode::TLightGroupGetOnCount, 0.0f))
		{
		case 0:
			if (normal)
				flash(island1);
			break;
		case 1:
			if (normal)
			{
				flash(island1);
				flash(island2);
			}
			index++;
			break;
		default:
			if (normal)
				flash(island);
			index += 2;
			break;
		}
		Display(Format(text, ScoreHit(caller, index)).c_str(), 2.0f, 0);
	}

	void Rule_75f1(MessageCode code, TPinballComponent* caller)
	{
		if (code == MessageCode::Ft1RampEnter) // FT1 84
		{
			Send(a_blocg, MessageCode::Ft1BlockerSetPosition, -1.0f);
			return;
		}
		IslandRamp(code, caller, left_island_combo_light_group, v_lite629, v_lite630, left_island_light_group,
		           v_lite603, v_lite604, 3004);
	}

	void Rule_78dd(MessageCode code, TPinballComponent* caller)
	{
		IslandRamp(code, caller, right_island_combo_light_group, v_lite626, v_lite627, right_wind_light_group,
		           v_lite611, v_lite612, 3005);
	}

	// 23:7bab - a_ramp13: line 2 (FT1 85) its score (text 3006, tabb41); made (FT1 86) blocker c away
	void Rule_7bab(MessageCode code, TPinballComponent* caller)
	{
		if (code == MessageCode::Ft1RampExit2)
		{
			Display(Format(3006, ScoreHit(caller, 0)).c_str(), 2.0f, 0);
			Play(tabb41wave);
		}
		else if (code == MessageCode::Ft1RampExit3)
			Send(a_blocc, MessageCode::Ft1BlockerSetPosition, -1.0f);
	}

	void Rule_98d7(MessageCode, TPinballComponent*) {} // TODO(23:98d7): s_oneway3
	// 23:472d - the bumpers: in state 9 each hit 25,000 (tabb74) and every tenth the jackpot; in normal play every
	// twelfth hit (a_bump1's MessageField) raises the bumpers' level (a_bump2's, up to 6; text 3001 with the new
	// worth, tabb17); the hit scores the level's worth (tabb16)
	void Rule_472d(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		switch (InfoField())
		{
		case 9:
			if (++g_651c == 10)
			{
				g_651c = 0;
				CollectJackpot(0.0f);
				return;
			}
			ScorePoints(caller, 25000);
			Play(tabb74wave);
			return;
		case 2:
		{
			auto count = Field(a_bump1) + 1;
			if (count == 12)
			{
				Field(a_bump1) = 0;
				auto level = Field(a_bump2);
				if (level < 6)
				{
					Field(a_bump2) = ++level;
					Display(Format(3001, caller ? caller->get_scoring(level) : 0).c_str(), 2.0f, 0);
				}
				Play(tabb17wave);
			}
			else
			{
				Play(tabb16wave);
				Field(a_bump1) = count;
			}
			break;
		}
		default:
			Play(tabb16wave);
			break;
		}
		if (caller)
			ScoreHit(caller, Field(a_bump2));
	}

	int g_653e = 0; // the drained ball: 1 lost, 3 saved (relaunched), 2 served again

	// 23:4448 - the drain. A ball while the ball saver (v_lite569) is on comes back (text 3026); otherwise the
	// mission system hears event 83; on the last ball of the last player (once, g_22f2) the game is tilted to let the
	// kept balls go (state 17); in a multiball nothing more; a treasure just ended clears the coins and special
	// treasures; then the end of the ball (state 16)
	void Rule_4448(MessageCode code, TPinballComponent* caller)
	{
		auto t = control::TableG;
		if (code == MessageCode::ControlCollision)
		{
			g_653e = 1;
			if (t->TiltLockFlag)
			{
				Send(v_lite569, MessageCode::TLightResetAndTurnOff, 0.0f);
				g_6522 = 0;
			}
			if (LightOn(v_lite569))
			{
				Display(ft1_strings::Get(3026), 2.0f, 0);
				g_653e = 3;
				// FT1 sends 1026 to the drain and sets table +0x5a: the ball is not lost. Here: the drain's timer stops
				Send(drain, MessageCode::Reset, 0.0f);
				Rule_4448(MessageCode::ControlTimerExpired, caller);
				return;
			}
			MissionEvent(FT1Event83, caller);
			if (!t->MultiballCount && !t->ExtraBalls && t->BallCount == 1 && t->CurrentPlayer + 1 == t->PlayerCount &&
				!g_22f2)
			{
				g_22f2 = 1;
				t->TiltLockFlag = 1;
				for (auto flipper : {&a_flip1, &a_flip3, &a_flip2, &a_flip4})
					Send(*flipper, MessageCode::SetTiltLock, 0.0f); // FT1 1011
				SetInfoField(17);
				MissionEvent(FT1Event79, nullptr);
			}
			if (t->MultiballCount && !g_22f2)
			{
				g_653e = 0;
				Send(drain, MessageCode::Reset, 0.0f); // FT1 1026
				return;
			}
			if (g_22f6)
			{
				g_22f6 = 0;
				Send(coin_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(special_treasure_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			}
			if (InfoField() != 16 && InfoField() != 1)
			{
				SetInfoField(16);
				MissionEvent(FT1Event79, nullptr);
			}
		}
		else if (code == MessageCode::ControlTimerExpired)
		{
			if (g_22f2)
				return;
			if (g_653e == 3)
				Send(s_launcher1, MessageCode::PlungerRelaunchBall, 0.96f); // FT1 1019
			else if (g_653e == 2)
				Send(s_launcher1, MessageCode::PlungerStartFeedTimer, 0.96f); // FT1 1017
		}
	}

	// 23:4152 - s_launcher1: a new ball is served (FT1 1017) - from normal play (g_6522 0) back to the skill shot
	// (state 3), the magnets off; the ball saver (v_lite569) on, v_lite544 lit, kicker 2 and wall 610 on, the fathoms
	// out (multiplier 1), kicker 1 off, the plunger blocker back. The feed (FT1 1016): event 80, the ball saver's
	// light as its stage has it. The plunger let go (FT1 1005, not tilted): the cannon fires
	void Rule_4152(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		switch (code)
		{
		case MessageCode::PlungerStartFeedTimer:
			g_22ee = 0;
			if (!g_6522)
			{
				g_6518 = 0;
				SetInfoField(3);
				MissionEvent(FT1Event79, nullptr);
				MagnetsOff();
				Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
				Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			}
			Send(v_lite569, MessageCode::TLightResetAndTurnOn, 0.0f);
			g_6522 = 3;
			Send(v_lite544, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			for (auto part : {&a_kick2, &s_bwal610})
				if (auto comp = part->GetComponent())
					comp->ActiveFlag = 1;
			Send(fathom_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Multiplier = 1; // table +0x122
			Send(a_kick1, MessageCode::Ft1Deactivate, 0.0f); // FT1 76
			Send(a_bloce, MessageCode::Ft1BlockerSetPosition, 0.0f);
			Send(v_lite504, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite505, MessageCode::TLightResetAndTurnOn, 0.0f);
			break;
		case MessageCode::PlungerFeedBall:
		{
			MissionEvent(FT1Event80, nullptr);
			auto light = v_lite569.Component;
			if (g_6522 > 3 || g_6522 < 0)
				g_6522 = 3;
			switch (g_6522)
			{
			case 3:
				if (light && !light->LightOnFlag) // +0x3a
					Send(v_lite569, MessageCode::TLightResetAndTurnOn, 0.0f);
				break;
			case 2:
				if (light && !light->ToggledOnFlag) // +0x40
					Send(v_lite569, MessageCode::TLightTurnOnTimed, 10.0f);
				break;
			case 1:
				if (light && !light->FlasherOnFlag) // +0x3c
					Send(v_lite569, MessageCode::TLightFlasherStartTimed, 5.0f);
				break;
			default:
				Send(v_lite569, MessageCode::TLightResetAndTurnOff, 0.0f);
				break;
			}
			break;
		}
		case MessageCode::PlungerInputReleased: // FT1 1005
			if (t->TiltLockFlag)
				break;
			Send(a_cannon, MessageCode::Ft1BlockerSetPosition, 3.0f);
			Send(a_cannon, MessageCode::Ft1BlockerMoveTo, 0.0f);
			break;
		default:
			break;
		}
	}

	// 23:43b9 - v_lite569, the ball saver: on 10 s after the launch, flashing 5 s, then off (g_6522 3 -> 2 -> 1 -> 0)
	void Rule_43b9(MessageCode code, TPinballComponent*)
	{
		if (code != MessageCode::ControlTimerExpired)
			return;
		switch (g_6522)
		{
		case 3:
			g_6522 = 2;
			Send(v_lite569, MessageCode::TLightTurnOnTimed, 10.0f);
			break;
		case 2:
			g_6522 = 1;
			Send(v_lite569, MessageCode::TLightFlasherStartTimed, 5.0f);
			break;
		case 1:
			g_6522 = 0;
			Send(v_lite569, MessageCode::TLightResetAndTurnOff, 0.0f);
			break;
		default:
			break;
		}
	}
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
