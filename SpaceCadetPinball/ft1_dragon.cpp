// Dragon's Keep rules, translated from DRAGON.EXE code segment 23 (see ft1_dragon.h).
//
// Conventions of the translation
// - Each function is named after what it does and carries its original address: "23:0778".
// - Data globals of the original keep their data-segment address in their name (g_5fdc).
// - Message codes are this engine's names; the Full Tilt 1 numbers they come from are in ft2/ft1_engine.md.
// - Original table fields (TPinballTable in the 16-bit engine): +0x122 multiplier (1-based), +0x124 added score,
//   +0x12c bonus, +0x130 bonus flag, +0x132 jackpot, +0x136 jackpot flag, +0x138 super jackpot, +0x13c its flag,
//   +0x140 ball count, +0x144 extra balls, +0x146 multiball count, +0x15a tilt lock.
// - TODO markers name what is not translated yet; nothing is filled in by guesswork.

#include "pch.h"
#include "ft1_dragon.h"

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
#include "TSoloTarget.h"
#include "TPopupTarget.h"
#include "TSink.h"
#include "THole.h"
#include "TDrain.h"
#include "TBumper.h"
#include "TKickout.h"
#include "TKickback.h"
#include "TRollover.h"
#include "TFlagSpinner.h"
#include "TLightRollover.h"
#include "TRamp.h"
#include "TTripwire.h"
#include "TOneway.h"
#include "TSound.h"
#include "high_score.h"
#include "TTextBox.h"

namespace
{
	// Full Tilt 1 message without an equivalent in this engine: sent by the rules to the component that scored last
	// when the ball scores somewhere else (FT1 code 77)
	const MessageCode LastHitMovedOn = static_cast<MessageCode>(2077);

	bool Active = false;
	int Translated = 0;

	// ---- components ----
	component_tag<TLight> v_lite28{"v_lite28"}, v_lite29{"v_lite29"}, v_lite30{"v_lite30"}, v_lite31{"v_lite31"},
	                      v_lite32{"v_lite32"}, v_lite33{"v_lite33"}, v_lite34{"v_lite34"}, v_lite35{"v_lite35"},
	                      v_lite36{"v_lite36"}, v_lite37{"v_lite37"}, v_lite38{"v_lite38"}, v_lite39{"v_lite39"};
	component_tag<TLightGroup> dragon_egg_light_group{"dragon_egg_light_group"},
	                           outlane_kicker_light_group{"outlane_kicker_light_group"};
	component_tag<TSound> tabc21wave{"tabc21wave"}, tabc27wave{"tabc27wave"}, tabc35wave{"tabc35wave"},
	                      tabc39wave{"tabc39wave"}, tabc40wave{"tabc40wave"}, tabc41wave{"tabc41wave"},
	                      tabc42wave{"tabc42wave"};
	component_tag<TTextBox> info_text_box{"info_text_box"};
	component_tag<TLight> v_lite46{"v_lite46"}, v_lite48{"v_lite48"}, v_lite49{"v_lite49"}, v_lite50{"v_lite50"},
	                      v_lite71{"v_lite71"}, v_lite72{"v_lite72"}, v_lite73{"v_lite73"}, v_lite74{"v_lite74"},
	                      v_lite75{"v_lite75"};
	component_tag<TSound> tabc43wave{"tabc43wave"};
	component_tag<> a_kick1{"a_kick1"}, a_kout3{"a_kout3"}, a_blocf{"a_blocf"}, a_bloce{"a_bloce"},
	                a_center_post{"a_center_post"}, a_kout2{"a_kout2"}, a_rockb{"a_rockb"}, a_sink17{"a_sink17"},
	                a_sink18{"a_sink18"}, s_hole118{"s_hole118"};

	// More components: each DK_TAG registers itself for linking
	std::vector<component_tag_base*>& TagRegistry()
	{
		static std::vector<component_tag_base*> tags;
		return tags;
	}

	struct TagReg
	{
		explicit TagReg(component_tag_base* tag) { TagRegistry().push_back(tag); }
	};

#define DK_TAG(type, name) component_tag<type> name{#name}; TagReg reg_##name{&name};
	DK_TAG(TPinballComponent, a_sword)
	DK_TAG(TPinballComponent, a_spot19)
	DK_TAG(TPinballComponent, a_spot26)
	DK_TAG(TPinballComponent, a_spot28)
	DK_TAG(TLight, v_lite16)
	DK_TAG(TLight, v_lite17)
	DK_TAG(TLight, v_lite18)
	DK_TAG(TSound, tabc13wave)
	DK_TAG(TLight, v_lite47)
	DK_TAG(TPinballComponent, a_dvtraa)
	DK_TAG(TPinballComponent, a_dvtrab)
	DK_TAG(TPinballComponent, a_dvtrda)
	DK_TAG(TPinballComponent, a_dvtrdb)
	DK_TAG(TLight, v_lite125)
	DK_TAG(TLight, v_lite126)
	DK_TAG(TLight, v_lite76)
	DK_TAG(TLight, v_lite119)
	DK_TAG(TLightGroup, dragon_mouth_light_group)
	DK_TAG(TLightGroup, dragon_fire_light_group)
	DK_TAG(TLight, v_lite113)
	DK_TAG(TLight, v_lite114)
	DK_TAG(TLight, v_lite115)
	DK_TAG(TPinballComponent, s_bwal170)
	DK_TAG(TPinballComponent, a_ramp107)
	DK_TAG(TPinballComponent, a_jaw)
	DK_TAG(TLight, v_lite40)
	DK_TAG(TPinballComponent, a_dvtrba)
	DK_TAG(TPinballComponent, a_dvtrbb)
	DK_TAG(TPinballComponent, a_dvtrca)
	DK_TAG(TPinballComponent, a_dvtrcb)
	DK_TAG(TLight, v_lite45)
	DK_TAG(TPinballComponent, a_rocka)
	DK_TAG(TLightGroup, gargoyle_spot_light_group)
	DK_TAG(TLightGroup, spell_spot_light_group)
	DK_TAG(TLight, v_lite67)
	DK_TAG(TLight, v_lite68)
	DK_TAG(TLight, v_lite69)
	DK_TAG(TLight, v_lite70)
	DK_TAG(TPinballComponent, a_bump1)
	DK_TAG(TPinballComponent, drain)
	DK_TAG(TPinballComponent, s_plunger1)
	DK_TAG(TPinballComponent, a_flip1)
	DK_TAG(TPinballComponent, a_flip2)
	DK_TAG(TPinballComponent, a_flip3)
	DK_TAG(TPinballComponent, a_sink16)
	DK_TAG(TPinballComponent, a_sink1)
	DK_TAG(TPinballComponent, a_sink4)
	DK_TAG(TLightGroup, dragon_letter_light_group)
	DK_TAG(TLight, v_lite63)
	DK_TAG(TLight, v_lite64)
	DK_TAG(TPinballComponent, a_spot4)
	DK_TAG(TPinballComponent, a_spot5)
	DK_TAG(TPinballComponent, a_spot6)
	DK_TAG(TPinballComponent, a_spot7)
	DK_TAG(TPinballComponent, a_spot22)
	DK_TAG(TPinballComponent, mode_timer)
	DK_TAG(TLightGroup, gem_light_group)
	DK_TAG(TPinballComponent, light_sweeps)
	DK_TAG(TSound, tabc46wave)
	DK_TAG(TSound, tabc47wave)
	DK_TAG(TLight, v_lite88)
	DK_TAG(TLight, v_lite89)
	DK_TAG(TLight, v_lite90)
	DK_TAG(TLight, v_lite91)
	DK_TAG(TLight, v_lite92)
	DK_TAG(TLight, v_lite93)
	DK_TAG(TLight, v_lite94)
	DK_TAG(TLight, v_lite95)
	DK_TAG(TLight, v_lite117)
	DK_TAG(TPinballComponent, a_accl1)
	DK_TAG(TPinballComponent, a_spot1)
	DK_TAG(TPinballComponent, a_spot2)
	DK_TAG(TPinballComponent, a_spot3)
	DK_TAG(TPinballComponent, a_spot8)
	DK_TAG(TPinballComponent, a_spot10)
	DK_TAG(TPinballComponent, a_spot12)
	DK_TAG(TPinballComponent, a_spot14)
	DK_TAG(TPinballComponent, a_sink2)
	DK_TAG(TPinballComponent, s_oneway5)
	DK_TAG(TPinballComponent, a_ramp103)
	DK_TAG(TPinballComponent, a_ramp6)
	DK_TAG(TSound, tabc48wave)
	DK_TAG(TLight, v_lite65)
	DK_TAG(TLight, v_lite66)
	DK_TAG(TSound, tabc50wave)
DK_TAG(TSound, tabc53wave)
DK_TAG(TSound, tabc55wave)
DK_TAG(TLight, v_lite14)
DK_TAG(TLightGroup, well_light_group)
DK_TAG(TSink, a_sink19)
DK_TAG(TSound, tabc03wave)
DK_TAG(TSound, tabc05wave)
DK_TAG(TSound, tabc07wave)
DK_TAG(TLight, v_lite120)
DK_TAG(TLight, v_lite121)
DK_TAG(TLight, v_lite3)
DK_TAG(TLight, v_lite4)
DK_TAG(TLight, v_lite5)
DK_TAG(TLight, v_lite6)
DK_TAG(TLight, v_lite7)
DK_TAG(TLight, v_lite8)
DK_TAG(TLight, v_lite9)
DK_TAG(TLight, v_lite10)
DK_TAG(TPinballComponent, a_bump3)
DK_TAG(TPinballComponent, a_bump4)
DK_TAG(TSound, tabc44wave)
DK_TAG(TSound, tabc56wave)
DK_TAG(TSound, tabc06wave)
DK_TAG(TSound, tabc61wave)
DK_TAG(TSound, tabc32wave)
DK_TAG(TSound, tabc33wave)
DK_TAG(TLight, v_lite57)
DK_TAG(TPinballComponent, s_oneway2)
DK_TAG(TLight, v_lite58)
DK_TAG(TLight, v_lite59)
DK_TAG(TLight, v_lite60)
DK_TAG(TLight, v_lite61)
DK_TAG(TLight, v_lite62)
DK_TAG(TLight, v_lite77)
DK_TAG(TLight, v_lite78)
DK_TAG(TLight, v_lite79)
DK_TAG(TSound, tabc24wave)
DK_TAG(TSound, tabc57wave)
DK_TAG(TSound, tabc58wave)
	DK_TAG(TLightGroup, prison_window_light_group)
	DK_TAG(TLightGroup, watch_window_light_group)
	DK_TAG(TLightGroup, gate_window_light_group)
	DK_TAG(TLight, v_lite22)
	DK_TAG(TLight, v_lite23)
	DK_TAG(TLight, v_lite24)
	DK_TAG(TLight, v_lite25)
	DK_TAG(TLight, v_lite26)
	DK_TAG(TLight, v_lite27)

	// ds:0x3514: the tags of lights 71..75, indexed by a_rockb's MessageField
	component_tag<TLight>* const QuestLights[5] = {&v_lite71, &v_lite72, &v_lite73, &v_lite74, &v_lite75};

	component_tag_base* AllTags[] =
	{
		&v_lite28, &v_lite29, &v_lite30, &v_lite31, &v_lite32, &v_lite33, &v_lite34, &v_lite35, &v_lite36, &v_lite37,
		&v_lite38, &v_lite39, &dragon_egg_light_group, &outlane_kicker_light_group, &tabc21wave, &tabc27wave,
		&tabc35wave, &tabc39wave, &tabc40wave, &tabc41wave, &tabc42wave, &info_text_box, &a_kick1, &a_kout3,
		&a_blocf, &a_bloce, &a_center_post, &v_lite46, &v_lite48, &v_lite49, &v_lite50, &v_lite71, &v_lite72,
		&v_lite73, &v_lite74, &v_lite75, &tabc43wave, &a_kout2, &a_rockb, &a_sink17, &a_sink18, &s_hole118,
	};

	// Full Tilt 1 light codes 24-26 (the light handler 10:04cc takes 0-26): the temporary overrides this engine has
	// for the Full Tilt edition of Space Cadet, which runs on the same engine
	const MessageCode FT1LightOverrideOff = MessageCode::TLightFtTmpOverrideOff; // FT1 25
	const MessageCode FT1LightResetOverride = MessageCode::TLightFtResetOverride; // FT1 26
	const MessageCode FT1LightOverrideOn = MessageCode::TLightFtTmpOverrideOn;    // FT1 24
	// Full Tilt 1 code 79: a rules-internal event for the mission system
	const MessageCode FT1Event79 = static_cast<MessageCode>(2079);

	// ---- data globals of the original ----
	TPinballComponent* g_5fdc = nullptr; // the component that scored last

	// Picture strings of the data segment (ds:0x2223..0x2241): one glyph each in the table's picture font
	const char* DataText(int address)
	{
		static const char* glyphs[] = {"$", "%", "&", "'", "(", "+", "*", ")", ",", "-", ".", "/", "0", "1", "2",
		                               "3"};
		auto index = (address - 0x2223) / 2;
		return index >= 0 && index < 16 ? glyphs[index] : "";
	}

	// ds:0x21d2 + n * 9: n dragon-egg pictures ("#" in the picture font)
	const char* EggText(int count)
	{
		static const char* eggs[] = {"", "#", "##", "###", "####", "#####", "######", "#######", "########"};
		return eggs[std::max(0, std::min(8, count))];
	}

	// ---- engine calls ----
	int Send(component_tag_base& tag, MessageCode code, float value)
	{
		auto comp = tag.GetComponent();
		return comp ? comp->Message(code, value) : 0;
	}

	void Play(component_tag<TSound>& sound)
	{
		if (sound.Component)
			sound.Component->Play(nullptr, "ft1_dragon");
	}

	// TTextBox::Display of the Full Tilt 1 engine (18:1270) has a third argument, the font: 0 the message font,
	// 1 the picture font (font1 in DRAGON.DAT: dragon, Jackpot, gems, weapons).
	// TODO(font): this engine draws every message with the message font; the picture font is not loaded yet.
	void Display(const char* text, float time, int font)
	{
		(void)font;
		if (info_text_box.Component)
			info_text_box.Component->Display(text, time);
	}

	bool LightOn(component_tag<TLight>& light) // +0x3a / +0x40 / +0x3c
	{
		return light.Component && light.Component->light_on();
	}

	// Full Tilt 1 rand(): 0..0x7fff (engine 1:21cd); the rules use rand() * n / 0x8000
	int Rand15()
	{
		return rand() & 0x7fff;
	}

	// The original engine's TPinballTable::AddScore (15:b8db). Unlike this engine's it multiplies by the multiplier
	// itself (1-based), caps the jackpot at 10,000,000 and has a second accumulator, the super jackpot (+0x138,
	// up to 20,000,000), filled while its flag (+0x13c) is set.
	int Multiplier = 1;           // table +0x122
	int SuperJackpot = 20000;     // table +0x138
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

	// ---- rules ----
	void ControlHandler(MessageCode code, TPinballComponent* cmp);
	void MissionEvent(MessageCode code, TPinballComponent* cmp);

	// 23:06cf - score a hit: tell the previous scorer the ball moved on, then add the component's score
	int ScoreHit(TPinballComponent* cmp, int index)
	{
		if (g_5fdc != cmp)
		{
			if (g_5fdc)
				ControlHandler(LastHitMovedOn, g_5fdc);
			g_5fdc = cmp;
		}
		return AddScore(cmp->get_scoring(index));
	}

	// 23:072c - the same with a given score
	int ScorePoints(TPinballComponent* cmp, int points)
	{
		if (g_5fdc != cmp)
		{
			if (g_5fdc)
				ControlHandler(LastHitMovedOn, g_5fdc);
			g_5fdc = cmp;
		}
		return AddScore(points);
	}

	// 23:0778 - a score without multiplier, bonus, jackpot or super jackpot (control::SpecialAddScore in Cadet's
	// rules); returns the points added
	int SpecialAddScore(TPinballComponent* cmp, int points)
	{
		auto t = control::TableG;
		auto bonusFlag = t->BonusScoreFlag;   // +0x130
		auto jackpotFlag = t->JackpotScoreFlag; // +0x136
		auto multiplier = Multiplier;         // +0x122
		if (g_5fdc != cmp)
		{
			if (g_5fdc)
				ControlHandler(LastHitMovedOn, g_5fdc);
			g_5fdc = cmp;
		}
		t->BonusScoreFlag = false;
		t->JackpotScoreFlag = false;
		Multiplier = 1;
		auto added = AddScore(points);
		t->BonusScoreFlag = bonusFlag;
		t->JackpotScoreFlag = jackpotFlag;
		Multiplier = multiplier;
		return added;
	}

	// 23:0824 - extra ball (the table holds at most one: ExtraBalls = 1)
	void AddExtraBall(float time)
	{
		control::TableG->ExtraBalls = 1;
		Play(tabc21wave);
		if (time > 0.0f)
			Display(ft1_strings::Get(3011), time, 0); // "Extra Ball"
	}

	// 23:08a4 - the outlane kicker is enabled
	void EnableKickback(bool announce)
	{
		if (a_kick1.GetComponent())
			a_kick1.GetComponent()->ActiveFlag = 1;
		if (v_lite37.Component)
			v_lite37.Component->MessageField = 0;
		Send(v_lite37, MessageCode::TLightFlasherStartTimedThenStayOn, 2.0f);
		Send(outlane_kicker_light_group, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		if (announce)
		{
			Display(DataText(0x222d), 2.0f, 1);
			Play(tabc41wave);
		}
	}

	// 23:0933 - dragon eggs: light `count` more, show them and score 100,000 per egg lit
	void AwardDragonEggs(int count)
	{
		for (auto n = count; n > 0; n--)
			Send(dragon_egg_light_group, MessageCode::TLightGroupResetAndTurnOn, 1.0f);
		Play(tabc27wave);
		auto eggs = Send(dragon_egg_light_group, MessageCode::TLightGroupGetOnCount, 0.0f);
		Display(EggText(eggs), 2.0f, 1);
		ScorePoints(nullptr, eggs * 100000);
	}

	// 23:09e5 - the next multiplier light (33..36): x2 25,000, x3 35,000, x4 45,000, x5 55,000
	void RaiseMultiplier()
	{
		const char* text;
		if (!LightOn(v_lite33))
		{
			Send(v_lite33, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Multiplier = 2;
			SpecialAddScore(nullptr, 25000);
			text = DataText(0x2225);
		}
		else if (!LightOn(v_lite34))
		{
			Send(v_lite34, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Multiplier = 3;
			SpecialAddScore(nullptr, 35000);
			text = DataText(0x2227);
		}
		else if (!LightOn(v_lite35))
		{
			Send(v_lite35, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Multiplier = 4;
			SpecialAddScore(nullptr, 45000);
			text = DataText(0x2229);
		}
		else if (!LightOn(v_lite36))
		{
			Send(v_lite36, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Multiplier = 5;
			SpecialAddScore(nullptr, 55000);
			text = DataText(0x222b);
		}
		else
		{
			Play(tabc39wave); // all four lit: 0b6f jumps past the display
			return;
		}
		Display(text, 2.0f, 1);
		Play(tabc39wave);
	}

	void BlocfRule(MessageCode code, TPinballComponent* caller);
	void BloceRule(MessageCode code, TPinballComponent* caller);
	void CenterPostRule(MessageCode code, TPinballComponent* caller);

	// 23:0bdf - a random weapon among those not yet lit: kickback (lite37) 25,000, center post (lite38) 50,000,
	// gate blocker (lite39) 100,000
	void RandomWeapon()
	{
		int choices[3], count = 0;
		if (!LightOn(v_lite37))
			choices[count++] = 0;
		if (!LightOn(v_lite38))
			choices[count++] = 1;
		if (!LightOn(v_lite39))
			choices[count++] = 2;
		if (!count)
			return;
		int points;
		switch (choices[Rand15() * count / 0x8000])
		{
		case 0:
			EnableKickback(true);
			points = 25000;
			break;
		case 1:
			Play(tabc40wave);
			CenterPostRule(MessageCode::TBlockerEnable, a_center_post.GetComponent()); // FT1 56
			Display(DataText(0x222f), 2.0f, 1);
			Play(tabc40wave);
			points = 50000;
			break;
		default:
			Play(tabc42wave);
			BloceRule(MessageCode::TBlockerEnable, a_bloce.GetComponent()); // FT1 56
			Display(DataText(0x2231), 2.0f, 1);
			Play(tabc42wave);
			points = 100000;
			break;
		}
		ScorePoints(nullptr, points);
	}

	// 23:0db4 - a random armor light among 28, 29, 31, 32 that is off: 0..3, or -1 if all are on
	int RandomArmor()
	{
		int choices[4], count = 0;
		if (!LightOn(v_lite28))
			choices[count++] = 0;
		if (!LightOn(v_lite29))
			choices[count++] = 1;
		if (!LightOn(v_lite31))
			choices[count++] = 2;
		if (!LightOn(v_lite32))
			choices[count++] = 3;
		if (!count)
			return -1;
		return choices[Rand15() * count / 0x8000];
	}

	int g_5fe4 = 0; // armor pieces awarded

	// 23:0ef0 - award armor piece `which` (0..3), 120,000
	void AwardArmor(int which)
	{
		g_5fe4++;
		switch (which)
		{
		case 0:
			Send(v_lite28, MessageCode::TLightTurnOnTimed, 30.0f);
			break;
		case 1:
			Send(v_lite29, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(v_lite30, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			if (a_kout3.GetComponent())
				a_kout3.GetComponent()->ActiveFlag = 1;
			break;
		case 2:
			Send(v_lite31, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			break;
		default:
			BlocfRule(MessageCode::TBlockerDisable, a_blocf.GetComponent()); // FT1 55
			break;
		}
		ScorePoints(nullptr, 120000);
		Play(tabc35wave);
		Display(DataText(0x2223), 2.0f, 1);
	}

	// 23:0fdd - all armor off
	void ResetArmor()
	{
		Send(v_lite28, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(v_lite29, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(v_lite30, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(a_kout3, MessageCode::Reset, 0.0f);
		Send(v_lite31, MessageCode::TLightResetAndTurnOff, 0.0f);
		BlocfRule(MessageCode::TBlockerEnable, a_blocf.GetComponent()); // FT1 56
	}

	// 23:107a - a random armor piece, if any is left
	void AwardRandomArmor()
	{
		auto which = RandomArmor();
		if (which != -1)
			AwardArmor(which);
	}

	// 23:1151 / 23:1187 - add to the super jackpot (max 20,000,000) / jackpot (max 10,000,000)
	void AddSuperJackpot(int points)
	{
		SuperJackpot = std::min(SuperJackpot + points, 20000000);
	}

	void AddJackpot(int points)
	{
		auto t = control::TableG;
		t->JackpotScore = std::min(t->JackpotScore + points, 10000000);
	}

	int& Field(component_tag_base& tag) // ->MessageField (+4)
	{
		static int none = 0;
		auto comp = tag.GetComponent();
		return comp ? comp->MessageField : (none = 0);
	}

	void SetActive(component_tag_base& tag, bool active) // byte +3
	{
		if (auto comp = tag.GetComponent())
			comp->ActiveFlag = active;
	}

	component_tag<TLight>& QuestLight() // ds:0x3514[a_rockb->MessageField]
	{
		return *QuestLights[std::max(0, std::min(4, Field(a_rockb)))];
	}

	// 23:151e - the three tower lock lights back to normal, sinks 17/18 inactive, counters cleared
	void ResetTowerLocks()
	{
		Send(v_lite48, FT1LightOverrideOff, 0.0f);
		Send(v_lite49, FT1LightOverrideOff, 0.0f);
		Send(v_lite50, FT1LightOverrideOff, 0.0f);
		SetActive(a_sink18, false);
		SetActive(a_sink17, false);
		Field(v_lite48) = 0;
		Field(v_lite49) = 0;
		Field(v_lite50) = 0;
	}

	// 23:197e - if a quest is running (a_kout2's MessageField), its light goes off and it ends
	void ClearQuestLight()
	{
		if (!Field(a_kout2))
			return;
		Send(QuestLight(), MessageCode::TLightResetAndTurnOff, 0.0f);
		Field(a_kout2) = 0;
	}

	int QuestStep19df();

	bool Flashing(component_tag<TLight>& light) // +0x3c
	{
		return light.Component && light.Component->FlasherOnFlag;
	}

	// 23:11bd - the sword and its three lights (16-18) for step -1..3; spots 19/26/28 are the targets that move it on
	void SwordStep(int step)
	{
		switch (step)
		{
		case 3:
			Send(a_sword, MessageCode::TBlockerEnable, 0.0f); // FT1 56: all the way up
			Send(a_spot19, MessageCode::TPopupTargetEnable, 0.0f);
			Send(a_spot26, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot28, MessageCode::TPopupTargetDisable, 0.0f);
			Send(v_lite16, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite17, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite18, MessageCode::TLightResetAndTurnOff, 0.0f);
			break;
		case 2:
			Play(tabc13wave);
			Send(a_sword, MessageCode::Ft1BlockerMoveTo, 1.0f);
			Send(a_spot19, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot26, MessageCode::TPopupTargetEnable, 0.0f);
			Send(a_spot28, MessageCode::TPopupTargetDisable, 0.0f);
			Send(v_lite16, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(v_lite17, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite18, MessageCode::TLightResetAndTurnOff, 0.0f);
			break;
		case 1:
			Play(tabc13wave);
			Send(a_sword, MessageCode::Ft1BlockerMoveTo, 0.0f);
			Send(a_spot19, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot26, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot28, MessageCode::TPopupTargetEnable, 0.0f);
			Send(v_lite16, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(v_lite17, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(v_lite18, MessageCode::TLightFlasherStartTimed, -1.0f);
			break;
		case 0:
			Play(tabc13wave);
			Send(a_sword, MessageCode::Ft1BlockerMoveTo, 0.0f);
			Send(a_spot19, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot26, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot28, MessageCode::TPopupTargetEnable, 0.0f);
			Send(v_lite16, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite17, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite18, MessageCode::TLightFlasherStartTimed, -1.0f);
			break;
		case -1:
			Send(a_sword, MessageCode::Ft1BlockerMoveTo, 0.0f);
			Send(a_spot19, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot26, MessageCode::TPopupTargetDisable, 0.0f);
			Send(a_spot28, MessageCode::TPopupTargetDisable, 0.0f);
			Send(v_lite16, MessageCode::TLightResetAndTurnOff, -1.0f);
			Send(v_lite17, MessageCode::TLightResetAndTurnOff, -1.0f);
			Send(v_lite18, MessageCode::TLightResetAndTurnOff, -1.0f);
			break;
		default:
			break;
		}
	}

	// 23:15b1 - the tower lock lights (48-50) back from their overrides; the one flashing opens its sink (48: sink 18,
	// 49: sink 17) and is marked in its MessageField; with all three on, the info box gets 13 and the mission system
	// event 79
	void UpdateTowerLocks()
	{
		Send(v_lite48, FT1LightResetOverride, 0.0f);
		Send(v_lite49, FT1LightResetOverride, 0.0f);
		Send(v_lite50, FT1LightResetOverride, 0.0f);
		Field(v_lite48) = Field(v_lite49) = Field(v_lite50) = 0;
		if (Flashing(v_lite48))
		{
			SetActive(a_sink18, true);
			SetActive(a_sink17, false);
			Field(v_lite48) = 1;
			return;
		}
		if (Flashing(v_lite49))
		{
			SetActive(a_sink18, false);
			SetActive(a_sink17, true);
			Field(v_lite49) = 1;
			return;
		}
		SetActive(a_sink18, false);
		SetActive(a_sink17, false);
		if (Flashing(v_lite50))
		{
			Field(v_lite50) = 1;
			return;
		}
		if (LightOn(v_lite48) && LightOn(v_lite49) && LightOn(v_lite50))
		{
			if (info_text_box.Component)
				info_text_box.Component->MessageField = 13;
			MissionEvent(FT1Event79, nullptr);
		}
	}

	// 23:17ec - the tower: v_lite47 on, the A and D diverters set, v_lite125 lit, the next tower lock light flashing
	void OpenTower()
	{
		Send(v_lite47, MessageCode::TLightResetAndTurnOn, 0.0f);
		Send(a_dvtraa, MessageCode::TGateDisable, 0.0f);
		Send(a_dvtrab, MessageCode::TGateEnable, 0.0f);
		Send(v_lite125, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		Send(v_lite126, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(a_dvtrda, MessageCode::TGateDisable, 0.0f);
		Send(a_dvtrdb, MessageCode::TGateEnable, 0.0f);
		if (!LightOn(v_lite48))
			Send(v_lite48, MessageCode::TLightFlasherStartTimed, -1.0f);
		else if (!LightOn(v_lite49))
			Send(v_lite49, MessageCode::TLightFlasherStartTimed, -1.0f);
		else if (!LightOn(v_lite50))
			Send(v_lite50, MessageCode::TLightFlasherStartTimed, -1.0f);
		UpdateTowerLocks();
	}

	// 23:1c81 - a quest completed: jackpot +500,000, one more quest done (v_lite28's MessageField counts them), its
	// light stays on
	void CompleteQuest()
	{
		if (Field(a_kout2))
		{
			AddJackpot(500000);
			Field(v_lite28)++;
			Send(QuestLight(), MessageCode::TLightResetAndTurnOn, 0.0f);
			Field(a_kout2) = 0;
		}
		QuestStep19df();
	}

	int g_2270 = 0;

	// 23:1eda - release the ball from the sink a component keeps in its MessageField (a pointer in the original);
	// with g_2270 set, s_hole118 lets its ball go instead (its MessageField 2, FT1 code 67)
	// TODO(23:1eda): this engine's MessageField is an int and cannot hold the sink; the callers' sinks are passed in
	void ReleaseTo(TPinballComponent* sink, float time)
	{
		if (g_2270)
		{
			Field(s_hole118) = 2;
			Send(s_hole118, MessageCode::Ft1HoleRelease, 0.0f);
			return;
		}
		if (sink)
			sink->Message(MessageCode::TSinkResetTimer, time);
	}

	// 23:21c5 - collect the jackpot (picture ds:21d0 when `show`); returns `time`
	float CollectJackpot(float time, bool show)
	{
		Play(tabc43wave);
		ScorePoints(nullptr, control::TableG->JackpotScore);
		if (show)
			Display("\"", 2.0f, 1);
		return time;
	}

	// 23:1f2c - a quest is running: the info box keeps its number, v_lite46 off, a_rockb raised in 2 s, the mission
	// system hears event 79; returns whether a quest was running
	bool QuestRunningStart()
	{
		if (!Field(a_kout2))
			return false;
		if (info_text_box.Component)
			info_text_box.Component->MessageField = Field(a_kout2);
		Send(v_lite46, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(a_rockb, MessageCode::TBlockerEnable, 2.0f); // FT1 56 (animated blocker, see ft2/ft1_engine.md)
		MissionEvent(FT1Event79, nullptr);
		return true;
	}

	int g_2272 = 0;

	// 23:19df - pick the next quest at random among those whose light (71-75) is still off: a_rockb's MessageField is
	// its light, a_kout2's the quest number (4-8), its light flashes. With all five done: the sword goes back, v_lite76
	// flashes, the info box gets 9 and the mission system event 79 (quest 9). Returns the quest, -1 when blocked
	int QuestStep19df()
	{
		if (g_2272)
			return -1;
		ClearQuestLight();
		int lights[5], quests[5], count = 0;
		for (int i = 0; i < 5; i++)
			if (!LightOn(*QuestLights[i]))
			{
				lights[count] = i;
				quests[count] = i + 4;
				count++;
			}
		if (!count)
		{
			SwordStep(-1);
			Send(v_lite76, MessageCode::TLightFlasherStartTimed, -1.0f);
			if (info_text_box.Component)
				info_text_box.Component->MessageField = 9;
			MissionEvent(FT1Event79, nullptr);
			return 9;
		}
		auto pick = Rand15() * count / 0x8000;
		Field(a_rockb) = lights[pick];
		Field(a_kout2) = quests[pick];
		Send(*QuestLights[lights[pick]], MessageCode::TLightFlasherStartTimed, -1.0f);
		return quests[pick];
	}

	int g_2274 = 0, g_226e = 0, g_226a = 0, g_2276 = 0, g_5fe0 = 0;

	// Music (12:112b loads, 12:1396 plays, 12:10fe the one playing): the table's tracks tabc1-7. 1fa4 keeps them in
	// g_2246.. (tabc7 g_225e is the table's own; a missing one falls back to it); here they are this engine's tracks
	enum MusicTrack { MusicMain = 7, MusicTrack1 = 1, MusicTrack2 = 2, MusicTrack4 = 4, MusicTrack5 = 5,
	                  MusicTrack6 = 6 };
	const MessageCode FT1GroupAnimationBackward = MessageCode::TLightGroupAnimationBackward; // FT1 30
	const MessageCode FT1GroupReset = MessageCode::TLightGroupReset;                         // FT1 38
	int MusicPlaying = 0;

	void PlayMusic(int track) // 12:1396
	{
		if (!midi::play_ft2_track(track))
			midi::play_ft2_track(MusicMain);
		MusicPlaying = track;
	}

	// 23:1d03 - the dragon asleep: its lights (119, the mouth and the fire) back from or to their override, the mouth
	// and fire out, the fire's notify timer reset, the mouth lights (113-115), s_bwal170 and a_ramp107 inactive, the
	// jaw closed (frame 0)
	void DragonSleeps(bool resetOverride)
	{
		g_2274 = 0;
		auto code = resetOverride ? FT1LightResetOverride : FT1LightOverrideOff;
		Send(v_lite119, code, 0.0f);
		Send(dragon_mouth_light_group, code, 0.0f);
		Send(dragon_fire_light_group, code, 0.0f);
		Send(dragon_mouth_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(dragon_fire_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(dragon_fire_light_group, MessageCode::TLightGroupRestartNotifyTimer, -1.0f);
		SetActive(v_lite113, false);
		SetActive(v_lite114, false);
		SetActive(v_lite115, false);
		SetActive(s_bwal170, false);
		SetActive(a_ramp107, false);
		Send(a_jaw, MessageCode::Ft1BlockerMoveTo, 0.0f);
	}

	// 23:1e31 - the dragon wakes: the mouth lights (113-115) active, v_lite119 held on, the mouth and fire lights
	// back from their override, the jaw opens (frame 7)
	void DragonWakes()
	{
		g_2274 = 1;
		SetActive(v_lite113, true);
		SetActive(v_lite114, true);
		SetActive(v_lite115, true);
		Send(v_lite119, FT1LightOverrideOn, 0.0f);
		Send(dragon_mouth_light_group, FT1LightResetOverride, 0.0f);
		Send(dragon_fire_light_group, FT1LightResetOverride, 0.0f);
		g_226e = 1;
		Field(v_lite113) = -1;
		Send(a_jaw, MessageCode::Ft1BlockerMoveTo, 7.0f);
	}

	// 23:1fa4 - the table's own messages (control::table_control_handler). FT1 1015 (load the music) is done by this
	// engine; FT1 1024 is this engine's PlayerChanged
	void TableControl(MessageCode code)
	{
		switch (code)
		{
		case MessageCode::SetTiltLock:
			g_226a = 0;
			ResetArmor();
			SetActive(a_kick1, false);
			break;
		case MessageCode::PlayerChanged:
			g_5fe0 = 0;
			Send(v_lite40, MessageCode::TLightResetAndTurnOff, 0.0f);
			SetActive(v_lite113, false);
			SetActive(v_lite114, false);
			SetActive(v_lite115, false);
			break;
		case MessageCode::NewGame:
		{
			g_2276 = 0;
			SetActive(v_lite113, false);
			SetActive(v_lite114, false);
			SetActive(v_lite115, false);
			SwordStep(3);
			if (MusicPlaying != MusicMain)
				PlayMusic(MusicMain);
			g_5fe0 = 3;
			Send(v_lite40, MessageCode::TLightResetAndTurnOn, 0.0f);
			auto t = control::TableG;
			t->JackpotScore = 1000000; // +0x132
			SuperJackpot = 2500000;    // +0x138
			if (info_text_box.Component)
				info_text_box.Component->MessageField = 1;
			break;
		}
		default:
			break;
		}
	}

	// 23:2224 - all six gems (lights 22-27) lit: the info box gets 15 and the mission system event 79
	bool AllGems()
	{
		if (LightOn(v_lite22) && LightOn(v_lite23) && LightOn(v_lite24) && LightOn(v_lite25) && LightOn(v_lite26) &&
			LightOn(v_lite27))
		{
			if (info_text_box.Component)
				info_text_box.Component->MessageField = 15;
			MissionEvent(FT1Event79, nullptr);
			return true;
		}
		return false;
	}

	int g_5fe2 = 0, g_3528 = 0;
	const MessageCode FT1Event80 = static_cast<MessageCode>(2080); // the plunger feeds a ball
	const MessageCode FT1Event83 = static_cast<MessageCode>(2083); // a ball in the drain

	int InfoField()
	{
		return info_text_box.Component ? info_text_box.Component->MessageField : 0;
	}

	void SetInfoField(int value)
	{
		if (info_text_box.Component)
			info_text_box.Component->MessageField = value;
	}

	void LightRule40(MessageCode code, TPinballComponent* caller);

	// 23:236d - s_oneway2 (the shooter lane exit): a launched ball starts the ball saver (v_lite40)
	void OnewayRule2(MessageCode code, TPinballComponent* caller)
	{
		if (code == MessageCode::ControlCollision && g_5fe0 == 3)
			LightRule40(MessageCode::ControlTimerExpired, caller);
	}

	// 23:23b9 - s_plunger1: a new ball is served (FT1 1017) - everything back for the ball, the next quest picked;
	// FT1 1016 (feed) goes to the mission system as event 80
	void PlungerRule(MessageCode code, TPinballComponent* caller)
	{
		if (code == MessageCode::PlungerStartFeedTimer)
		{
			g_2272 = 0;
			g_226a = 0;
			ResetArmor();
			UpdateTowerLocks();
			DragonSleeps(true);
			CenterPostRule(MessageCode::TBlockerDisable, a_center_post.GetComponent()); // FT1 55
			BloceRule(MessageCode::TBlockerDisable, a_bloce.GetComponent());
			SetActive(a_kick1, true);
			Field(v_lite37) = 0;
			Send(v_lite37, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(outlane_kicker_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(outlane_kicker_light_group, MessageCode::TLightGroupOffsetAnimationForward, 0.0f);
			Send(v_lite33, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite34, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite35, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite36, MessageCode::TLightResetAndTurnOff, 0.0f);
			Multiplier = 1;
			// info box 6 and 13 (a multiball being set up) keep the tower's A diverter and the D diverter
			bool keep = InfoField() == 6 || InfoField() == 13;
			if (!keep)
			{
				Send(v_lite47, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(a_dvtraa, MessageCode::TGateEnable, 0.0f);
				Send(a_dvtrab, MessageCode::TGateDisable, 0.0f);
			}
			Send(a_dvtrba, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrbb, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrca, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrcb, MessageCode::TGateEnable, 0.0f);
			if (!keep)
			{
				Send(v_lite125, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(v_lite126, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
				Send(a_dvtrda, MessageCode::TGateEnable, 0.0f);
				Send(a_dvtrdb, MessageCode::TGateDisable, 0.0f);
			}
			SwordStep(3);
			Send(v_lite46, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_rockb, MessageCode::TBlockerEnable, 0.0f);
			Send(v_lite45, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_rocka, MessageCode::TBlockerEnable, 0.0f);
			Send(gargoyle_spot_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite67, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite70, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite68, MessageCode::TLightResetAndTurnOff, 0.0f);
			Field(v_lite68) = 0;
			Send(v_lite69, MessageCode::TLightResetAndTurnOff, 0.0f);
			Field(v_lite69) = 0;
			Send(spell_spot_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Field(a_bump1) = 0;
			g_5fe2 = 0;
			g_5fe4 = 0;
			g_5fdc = nullptr;
			if (QuestStep19df() != 9)
			{
				SetInfoField(10);
				MissionEvent(FT1Event79, nullptr);
			}
			Send(v_lite40, MessageCode::TLightResetAndTurnOn, 0.0f);
			g_5fe0 = 3;
		}
		else if (code == MessageCode::PlungerFeedBall)
			MissionEvent(FT1Event80, nullptr);
	}

	// 23:27bb - v_lite40, the ball saver: on 10 s after the launch, flashing 5 s, then off (g_5fe0 3 -> 2 -> 1 -> 0)
	void LightRule40(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlTimerExpired)
			return;
		switch (g_5fe0)
		{
		case 3:
			g_5fe0 = 2;
			Send(v_lite40, MessageCode::TLightTurnOnTimed, 10.0f);
			break;
		case 2:
			g_5fe0 = 1;
			Send(v_lite40, MessageCode::TLightFlasherStartTimed, 5.0f);
			break;
		case 1:
			g_5fe0 = 0;
			Send(v_lite40, MessageCode::TLightResetAndTurnOff, 0.0f);
			break;
		default:
			break;
		}
	}

	// 23:284a - the drain. A ball while the ball saver is on comes back (relaunched); otherwise the mission system
	// hears event 83; on the last ball of the last player of a game not yet extended (g_2276), the locked balls in
	// the tower (lights 48-50) are released: one more chance
	void DrainRule(MessageCode code, TPinballComponent* caller)
	{
		auto t = control::TableG;
		if (code == MessageCode::ControlCollision)
		{
			g_3528 = 1;
			if (t->TiltLockFlag)
			{
				Send(v_lite40, MessageCode::TLightResetAndTurnOff, 0.0f);
				g_5fe0 = 0;
			}
			if (LightOn(v_lite40))
			{
				g_3528 = 3;
				// FT1 sends 1011 to the drain and sets table +0x5a: the ball is not lost. Here: the drain's timer stops
				Send(drain, MessageCode::Reset, 0.0f);
				DrainRule(MessageCode::ControlTimerExpired, caller);
				return;
			}
			MissionEvent(FT1Event83, caller);
			if (!t->MultiballCount && !t->ExtraBalls && t->BallCount == 1 && t->CurrentPlayer + 1 == t->PlayerCount &&
				!g_2276)
			{
				g_2276 = (t->TiltLockFlag ? 1 : 0) + 1;
				t->TiltLockFlag = 1;
				Send(a_flip1, MessageCode::SetTiltLock, 0.0f);
				Send(a_flip3, MessageCode::SetTiltLock, 0.0f);
				Send(a_flip2, MessageCode::SetTiltLock, 0.0f);
				if (v_lite50.Component && v_lite50.Component->LightOnFlag)
				{
					SetInfoField(1);
					t->MultiballCount++;
					Send(a_sink16, MessageCode::TSinkResetTimer, 0.0f);
					Send(v_lite50, MessageCode::TLightResetAndTurnOff, 0.0f);
				}
				if (v_lite49.Component && v_lite49.Component->LightOnFlag)
				{
					SetInfoField(1);
					t->MultiballCount++;
					Send(a_sink17, MessageCode::TSinkResetTimer, 0.0f);
					Send(v_lite49, MessageCode::TLightResetAndTurnOff, 0.0f);
				}
				if (v_lite48.Component && v_lite48.Component->LightOnFlag)
				{
					SetInfoField(1);
					t->MultiballCount++;
					Send(a_sink18, MessageCode::TSinkResetTimer, 0.0f);
					Send(v_lite48, MessageCode::TLightResetAndTurnOff, 0.0f);
				}
			}
			if (t->MultiballCount && !g_2276)
			{
				g_3528 = 0;
				return;
			}
			if (InfoField() != 11 && InfoField() != 0)
			{
				SetInfoField(11);
				MissionEvent(FT1Event79, nullptr);
			}
		}
		else if (code == MessageCode::ControlTimerExpired)
		{
			if (g_2276)
				return;
			if (g_3528 == 3)
				Send(s_plunger1, MessageCode::PlungerRelaunchBall, 0.96f); // FT1 1019
			else if (g_3528 == 2)
				Send(s_plunger1, MessageCode::PlungerStartFeedTimer, 0.96f); // FT1 1017
		}
	}

	// ---- not translated yet ----
	// The mission handlers, one per info box state (23:c716's table at cs:c883)
	void Mission0(MessageCode code, TPinballComponent* cmp);  // 23:c4c7
	void Mission2(MessageCode code, TPinballComponent* cmp);  // 23:7f7d
	void Mission3(MessageCode code, TPinballComponent* cmp);  // 23:802d
	void Mission4(MessageCode code, TPinballComponent* cmp);  // 23:82a6
	void Mission5(MessageCode code, TPinballComponent* cmp);  // 23:8b1b
	void Mission6(MessageCode code, TPinballComponent* cmp);  // 23:8f75
	void Mission7(MessageCode code, TPinballComponent* cmp);  // 23:978f
	void Mission8(MessageCode code, TPinballComponent* cmp);  // 23:9c76
	void Mission9(MessageCode code, TPinballComponent* cmp);  // 23:9fae
	void Mission10(MessageCode code, TPinballComponent* cmp); // 23:7da7
	void Mission11(MessageCode code, TPinballComponent* cmp); // 23:be51
	void Mission12(MessageCode code, TPinballComponent* cmp); // 23:ae65
	void Mission13(MessageCode code, TPinballComponent* cmp); // 23:b47c
	void Mission14(MessageCode code, TPinballComponent* cmp); // 23:b89a
	void Mission15(MessageCode code, TPinballComponent* cmp); // 23:ba66
	void MissionOther(MessageCode code, TPinballComponent* cmp); // 23:c5a9

	// 23:c716
	void MissionDispatch(int state, MessageCode code, TPinballComponent* cmp)
	{
		switch (state)
		{
		case 0: Mission0(code, cmp); break;
		case 1: break;
		case 2: Mission2(code, cmp); break;
		case 3: Mission3(code, cmp); break;
		case 4: Mission4(code, cmp); break;
		case 5: Mission5(code, cmp); break;
		case 6: Mission6(code, cmp); break;
		case 7: Mission7(code, cmp); break;
		case 8: Mission8(code, cmp); break;
		case 9: Mission9(code, cmp); break;
		case 10: Mission10(code, cmp); break;
		case 11: Mission11(code, cmp); break;
		case 12: Mission12(code, cmp); break;
		case 13: Mission13(code, cmp); break;
		case 14: Mission14(code, cmp); break;
		case 15: Mission15(code, cmp); break;
		default: MissionOther(code, cmp); break;
		}
	}

	// 23:c5d6 - the mission system hears every message first. A ball in the drain with no other ball blocks the next
	// quest (g_2272); with v_lite28 lit, solo targets add 100,000 to the super jackpot; popup and solo targets add
	// 50,000 to the jackpot (and count in g_5fe2), sinks 1 and 4 75,000. Then the handler of the info box's state
	// gets the message; FT1 1009 (resume) and the info box's own timer with nothing queued (+0x4e) become event 80
	void MissionEvent(MessageCode code, TPinballComponent* cmp)
	{
		if (!info_text_box.Component)
			return;
		auto t = control::TableG;
		if (cmp && cmp == drain.GetComponent() && code == MessageCode::ControlCollision && !t->MultiballCount)
			g_2272 = 1;
		if (cmp && LightOn(v_lite28) && dynamic_cast<TSoloTarget*>(cmp))
			AddSuperJackpot(100000);
		if (cmp)
		{
			if (dynamic_cast<TPopupTarget*>(cmp) || dynamic_cast<TSoloTarget*>(cmp))
			{
				g_5fe2++;
				AddJackpot(50000);
			}
			else if (dynamic_cast<TSink*>(cmp) && (cmp == a_sink4.GetComponent() || cmp == a_sink1.GetComponent()))
				AddJackpot(75000);
		}
		auto state = info_text_box.Component->MessageField;
		// TODO(+0x4e): the FT1 text box's "nothing queued" flag; this engine's text box has no queue flag
		if (code == MessageCode::Resume ||
			(code == MessageCode::ControlTimerExpired && cmp == info_text_box.Component))
			code = FT1Event80;
		MissionDispatch(state, code, cmp);
	}
	// 23:4215 / 23:436f - the center post (light v_lite38) and a_bloce (v_lite39): 56 raises it for its first
	// duration (the light flashes 2 s and stays on), then the light flashes out over the second; 55 or the end lowers
	// it. The blocker's MessageField: 0 down, 1 up, 2 going.
	void RaisedBlockerRule(MessageCode code, TPinballComponent* caller, component_tag<TLight>& light)
	{
		auto blocker = dynamic_cast<TBlocker*>(caller);
		if (!blocker)
			return;
		switch (code)
		{
		case MessageCode::TBlockerEnable: // FT1 56
			blocker->MessageField = 1;
			blocker->Message(MessageCode::TBlockerEnable, 0.0f);
			blocker->Message(MessageCode::Ft1BlockerTimer, static_cast<float>(blocker->InitialDuration));
			Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 2.0f);
			break;
		case MessageCode::TBlockerDisable: // FT1 55
			blocker->MessageField = 0;
			blocker->Message(MessageCode::TBlockerDisable, 0.0f);
			Send(light, MessageCode::TLightResetAndTurnOff, 0.0f);
			break;
		case MessageCode::ControlTimerExpired: // FT1 70
			if (blocker->MessageField == 1)
			{
				blocker->MessageField = 2;
				blocker->Message(MessageCode::Ft1BlockerTimer, static_cast<float>(blocker->ExtendedDuration));
				Send(light, MessageCode::TLightFlasherStartTimedThenStayOff,
				     static_cast<float>(blocker->ExtendedDuration));
			}
			else
			{
				blocker->MessageField = 0;
				blocker->Message(MessageCode::TBlockerDisable, 0.0f);
			}
			break;
		default:
			break;
		}
	}

	void CenterPostRule(MessageCode code, TPinballComponent* caller) // 23:4215
	{
		RaisedBlockerRule(code, caller, v_lite38);
	}

	void BloceRule(MessageCode code, TPinballComponent* caller) // 23:436f
	{
		RaisedBlockerRule(code, caller, v_lite39);
	}

	// 23:44c9 - a_blocf, the other way round (light v_lite32): 55 lowers it for its first duration (the light on for
	// that long), then the light flashes for the second; then it comes back up. 56 brings it straight back.
	void BlocfRule(MessageCode code, TPinballComponent* caller)
	{
		auto blocker = dynamic_cast<TBlocker*>(caller);
		if (!blocker)
			return;
		switch (code)
		{
		case MessageCode::TBlockerDisable: // FT1 55
			blocker->MessageField = 1;
			blocker->Message(MessageCode::TBlockerDisable, 0.0f);
			blocker->Message(MessageCode::Ft1BlockerTimer, static_cast<float>(blocker->InitialDuration));
			Send(v_lite32, MessageCode::TLightTurnOnTimed, static_cast<float>(blocker->InitialDuration));
			break;
		case MessageCode::ControlTimerExpired: // FT1 70
			if (blocker->MessageField == 1)
			{
				blocker->MessageField = 2;
				blocker->Message(MessageCode::Ft1BlockerTimer, static_cast<float>(blocker->ExtendedDuration));
				Send(v_lite32, MessageCode::TLightFlasherStartTimed, static_cast<float>(blocker->ExtendedDuration));
				break;
			}
			blocker->MessageField = 0;
			blocker->Message(MessageCode::TBlockerEnable, 0.0f);
			break;
		case MessageCode::TBlockerEnable: // FT1 56
			blocker->Message(MessageCode::Ft1BlockerTimer, -1.0f);
			Send(v_lite32, MessageCode::TLightResetAndTurnOff, 0.0f);
			blocker->MessageField = 0;
			blocker->Message(MessageCode::TBlockerEnable, 0.0f);
			break;
		default:
			break;
		}
	}

	// FT1 code 66: a hole (s_hole118) has the ball (15:667b); THole's ControlBallCaptured is passed on as 66
	// (ft1_dragon::Handle)
	const MessageCode FT1Code66 = static_cast<MessageCode>(2066);

	void InfoClear() // 18:0b0c
	{
		if (info_text_box.Component)
			info_text_box.Component->Clear();
	}

	void SetState(int state) // the info box's MessageField is the mission state; the handlers hear event 79
	{
		SetInfoField(state);
		MissionEvent(FT1Event79, nullptr);
	}

	// 23:7d0e - the tower lock lights reset and the dragon letters back from their override
	void ResetTowerAndLetters()
	{
		ResetTowerLocks();
		Send(dragon_letter_light_group, FT1LightOverrideOff, 0.0f);
	}

	// 23:7d3c - the info box cleared, lights 63/64 off, the dragon letters' override reset, the tower locks updated
	void ClearLettersAndLocks()
	{
		InfoClear();
		Send(v_lite63, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(v_lite64, MessageCode::TLightResetAndTurnOff, 0.0f);
		Send(dragon_letter_light_group, FT1LightResetOverride, 0.0f);
		UpdateTowerLocks();
	}

	// 23:7da7 - state 10, the skill shot after a new ball (text 3066): the first part hit decides. Solo targets: spot4
	// an armor piece, spot5 a dragon egg, spot6 the multiplier, spot7 a weapon, spot22 state 14; other parts 15,000;
	// then state 2 (normal play). s_hole118 (FT1 66) starts the quest instead
	void Mission10(MessageCode code, TPinballComponent* cmp)
	{
		auto t = control::TableG;
		auto toPlay = [&](bool score)
		{
			if (score && !t->TiltLockFlag)
				ScorePoints(nullptr, 15000);
			SetState(2);
			MissionEvent(FT1Event80, nullptr);
		};
		if (code == FT1Event79)
			g_2270 = 1;
		if (code == FT1Event79 || code == FT1Event80)
		{
			Display(ft1_strings::Get(3066), -1.0f, 0);
			return;
		}
		if (code == FT1Code66)
		{
			if (cmp == s_hole118.GetComponent())
				QuestRunningStart();
			else
				toPlay(true);
			return;
		}
		if (code != MessageCode::ControlCollision || !cmp)
			return;
		if (dynamic_cast<TSoloTarget*>(cmp)) // FT1 type 1019
		{
			if (cmp == a_spot4.GetComponent())
				AwardRandomArmor();
			else if (cmp == a_spot5.GetComponent())
				AwardDragonEggs(1);
			else if (cmp == a_spot6.GetComponent())
				RaiseMultiplier();
			else if (cmp == a_spot7.GetComponent())
				RandomWeapon();
			else if (cmp == a_spot22.GetComponent())
			{
				SetState(14);
				return;
			}
			else
			{
				toPlay(true);
				return;
			}
			SetState(2);
			return;
		}
		if (dynamic_cast<TDrain*>(cmp)) // type 1007
		{
			toPlay(false);
			return;
		}
		// types 1000, 1005, 1006, 1012, 1014, 1015, 1017, 1018, 1020, 1021, 1024, 1030: 15,000
		if (dynamic_cast<TBumper*>(cmp) || dynamic_cast<TPopupTarget*>(cmp) || dynamic_cast<TKickout*>(cmp) ||
			dynamic_cast<TKickback*>(cmp) || dynamic_cast<TRollover*>(cmp) || dynamic_cast<TSink*>(cmp) ||
			dynamic_cast<TFlagSpinner*>(cmp) || dynamic_cast<TLightRollover*>(cmp) || dynamic_cast<TRamp*>(cmp) ||
			dynamic_cast<TTripwire*>(cmp))
			toPlay(true);
	}

	// 23:7f7d - state 2, normal play: text 3001; on entering, the info box cleared (unless g_2270) and the table's
	// music back
	void Mission2(MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event80)
			Display(ft1_strings::Get(3001), -1.0f, 0);
		else if (code == FT1Event79)
		{
			if (!g_2270)
				InfoClear();
			g_2270 = 0;
			if (MusicPlaying != MusicMain)
				PlayMusic(MusicMain);
		}
	}

	// Full Tilt 1 events between the mission handlers
	const MessageCode FT1Event81 = static_cast<MessageCode>(2081); // a paused quest goes on
	const MessageCode FT1Event82 = static_cast<MessageCode>(2082); // a quest is over
	// FT1 code 86 from a ramp: the ramp made. TODO(86): here TRamp's ControlCollision is passed on as 86
	const MessageCode FT1Code86 = MessageCode::Ft1RampExit3; // FT1 86: a ramp made (TRamp::Ft1Events)
	const MessageCode FT1LightSweepStart = MessageCode::Ft1LightSweepStart; // FT1 27 to light_sweeps (type 1038)

	// 1:1dcb (sprintf) of a table text with one number
	std::string Format(int textId, long long value)
	{
		char buffer[256];
		snprintf(buffer, sizeof buffer, ft1_strings::Get(textId), static_cast<int>(value));
		return buffer;
	}

	int TimeLeftMs(component_tag_base& timerTag) // FT1 72
	{
		return Send(timerTag, MessageCode::Ft1TimerTimeLeft, 0.0f);
	}

	bool TextQueued() // the FT1 text box's +0x4e. TODO(+0x4e): this engine's text box has no queue flag
	{
		return false;
	}

	// 23:802d - state 3, a quest's hurry-up: lights 63/64 flash and the mode timer gives 10 s to get back into it
	// (text 3067 counts down). Run out or drained: the paused quest (kept in the mode timer's MessageField) hears 82
	void Mission3(MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event79)
		{
			Send(v_lite63, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite64, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 10.0f);
		}
		if (code == FT1Event79 || code == FT1Event80)
		{
			auto left = TimeLeftMs(mode_timer);
			if (left)
				Display(Format(3067, left / 1000).c_str(), 0.5f, 0);
			return;
		}
		if (code == FT1Event83)
		{
			MissionDispatch(Field(mode_timer), FT1Event82, nullptr);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			Field(mode_timer) = -1;
			SetInfoField(2);
			return;
		}
		if (code == MessageCode::ControlTimerExpired && cmp == mode_timer.GetComponent())
		{
			Send(v_lite63, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite64, MessageCode::TLightResetAndTurnOff, 0.0f);
			MissionDispatch(Field(mode_timer), FT1Event82, nullptr);
			Field(mode_timer) = -1;
			if (InfoField() == 3)
				SetState(2);
		}
	}

	// 23:8236 - one of the nine gem shots (bits in v_lite88's MessageField); all nine: the gems flash and an extra
	// ball is lit
	void GemShot(int bit)
	{
		auto& gems = Field(v_lite88);
		gems |= bit;
		if (gems == 0x1ff)
		{
			Send(gem_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			AddExtraBall(2.0f);
		}
		else
			Play(tabc47wave);
	}

	// 23:82a6 - state 4, the gem quest (text 3020, music tabc2): nine shots, each with its gem light and points;
	// 15 s on the mode timer, then the hurry-up (state 3)
	void Mission4(MessageCode code, TPinballComponent* cmp)
	{
		auto gem = [&](component_tag<TLight>& light, int points, int textId, int bit)
		{
			Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			auto scored = ScorePoints(cmp, points);
			Display(Format(textId, scored).c_str(), 2.0f, 0);
			GemShot(bit);
		};
		if (code == FT1Event79)
		{
			Send(light_sweeps, FT1LightSweepStart, 0.0f);
			Send(gem_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(a_dvtrba, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrbb, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrca, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrcb, MessageCode::TGateDisable, 0.0f);
			ScorePoints(nullptr, 100000);
			InfoClear();
			Display(ft1_strings::Get(3020), -1.0f, 0);
			Field(v_lite88) = 0;
			PlayMusic(MusicTrack2);
			Play(tabc46wave);
			return;
		}
		if (code == FT1Event83 || code == FT1Event82)
		{
			if (code == FT1Event83)
			{
				SetInfoField(2);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			}
			ClearLettersAndLocks();
			Send(gem_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(gem_light_group, FT1LightResetOverride, 0.0f);
			CompleteQuest();
			return;
		}
		if (code == FT1Event81)
		{
			Play(tabc46wave);
			Send(gem_light_group, FT1LightResetOverride, 0.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 15.0f);
			if (TextQueued())
				return;
		}
		if (code == FT1Event81 || code == FT1Event80)
		{
			auto left = TimeLeftMs(mode_timer);
			if (left)
				Display(Format(3019, left / 1000).c_str(), 0.5f, 0);
			return;
		}
		if (code == MessageCode::ControlCollision && cmp)
		{
			if (dynamic_cast<TSink*>(cmp)) // 1017
			{
				if (cmp == a_sink1.GetComponent() || cmp == a_sink2.GetComponent())
					gem(v_lite91, 100000, 3024, 0x20);
			}
			else if (dynamic_cast<TBumper*>(cmp)) // 1005
				gem(v_lite88, 10000, 3021, 1);
			else if (dynamic_cast<TOneway*>(cmp)) // 1016
			{
				if (cmp == s_oneway5.GetComponent())
				{
					Send(v_lite95, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					CollectJackpot(0.0f, true);
					GemShot(0x40);
				}
			}
			else if (dynamic_cast<TSoloTarget*>(cmp)) // 1019
			{
				if (cmp == a_spot4.GetComponent() || cmp == a_spot5.GetComponent() || cmp == a_spot6.GetComponent() ||
					cmp == a_spot7.GetComponent() || cmp == a_spot22.GetComponent())
					gem(v_lite89, 50000, 3022, 2);
				else if (cmp == a_spot3.GetComponent() || cmp == a_spot2.GetComponent() || cmp == a_spot1.GetComponent())
					gem(v_lite90, 60000, 3023, 4);
				else if (cmp == a_spot10.GetComponent() || cmp == a_spot8.GetComponent() ||
					cmp == a_spot14.GetComponent() || cmp == a_spot12.GetComponent())
					gem(v_lite93, 70000, 3026, 0x10);
			}
			else if (cmp == a_accl1.GetComponent()) // 1036
				gem(v_lite117, 100000, 3028, 0x100);
			return;
		}
		if (code == FT1Code86)
		{
			if (cmp == a_ramp103.GetComponent())
				gem(v_lite94, 250000, 3027, 0x80);
			else if (cmp == a_ramp6.GetComponent())
				gem(v_lite92, 200000, 3025, 8);
			return;
		}
		if (code == MessageCode::ControlTimerExpired && cmp)
		{
			if (cmp == mode_timer.GetComponent())
			{
				Field(mode_timer) = InfoField();
				Send(gem_light_group, FT1LightOverrideOff, 0.0f);
				SetState(3);
			}
			else if (cmp == light_sweeps.GetComponent()) // 1038: the sweep is over, the quest starts
			{
				ResetTowerAndLetters();
				ReleaseTo(a_sink1.GetComponent(), 0.0f);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 27.73f);
				MissionEvent(FT1Event80, nullptr);
			}
		}
	}

	// The common end of a quest's ControlTimerExpired: the mode timer ran out (hurry-up, state 3; `lights` go out of
	// their override), or the light sweep finished (the quest starts: the ball comes out of sink 1, `seconds` on the
	// mode timer)
	void QuestTimers(TPinballComponent* cmp, std::initializer_list<component_tag_base*> lights, float seconds)
	{
		if (cmp == mode_timer.GetComponent())
		{
			Field(mode_timer) = InfoField();
			for (auto light : lights)
				Send(*light, FT1LightOverrideOff, 0.0f);
			SetState(3);
		}
		else if (cmp == light_sweeps.GetComponent())
		{
			ResetTowerAndLetters();
			ReleaseTo(a_sink1.GetComponent(), 0.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, seconds);
			MissionEvent(FT1Event80, nullptr);
		}
	}

	void Countdown(int textId) // the mode timer's seconds in a table text, for half a second
	{
		auto left = TimeLeftMs(mode_timer);
		if (left)
			Display(Format(textId, left / 1000).c_str(), 0.5f, 0);
	}

	// 23:8b1b - state 5, quest (text 3031, music tabc1): lights 63-66 and 91; sink 2's MessageField and v_lite91's
	// count the shots (their rules); 15 s again after a hurry-up; text 3030 counts down
	void Mission5(MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event79)
		{
			Send(light_sweeps, FT1LightSweepStart, 1.0f);
			Field(v_lite91) = 0;
			Field(a_sink2) = 0;
			ScorePoints(nullptr, 100000);
			InfoClear();
			Display(ft1_strings::Get(3031), -1.0f, 0);
			Send(v_lite63, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite64, MessageCode::TLightFlasherStartTimed, -1.0f);
			Play(tabc48wave);
			PlayMusic(MusicTrack1);
			return;
		}
		if (code == FT1Event83 || code == FT1Event82)
		{
			if (code == FT1Event83)
			{
				SetInfoField(2);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			}
			ClearLettersAndLocks();
			Send(v_lite63, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite64, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite66, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite66, FT1LightResetOverride, 0.0f);
			Send(v_lite65, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite65, FT1LightResetOverride, 0.0f);
			Send(v_lite91, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite91, FT1LightResetOverride, 0.0f);
			CompleteQuest();
			return;
		}
		if (code == FT1Event81)
		{
			Send(v_lite63, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite64, MessageCode::TLightFlasherStartTimed, -1.0f);
			Play(tabc48wave);
			Send(v_lite66, FT1LightResetOverride, 0.0f);
			Send(v_lite65, FT1LightResetOverride, 0.0f);
			Send(v_lite91, FT1LightResetOverride, 0.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 15.0f);
			if (TextQueued())
				return;
		}
		if (code == FT1Event81 || code == FT1Event80)
		{
			Countdown(3030);
			return;
		}
		if (code == MessageCode::ControlTimerExpired && cmp)
			QuestTimers(cmp, {&v_lite66, &v_lite65, &v_lite91}, 31.69f);
	}

	int g_5fea = 0; // how v_lite48 was when the prison quest started: 0 off, 1 on, 2 flashing

	// FT1 group codes 28-50 are this engine's 24-46
	const MessageCode FT1GroupRestartNotify = MessageCode::TLightGroupRestartNotifyTimer; // FT1 47

	// 23:8f75 - state 6, the prison (text 3033, music tabc4): the tower's diverters send the ball up; the four prison
	// windows (their group chases every 0.2 s, an unlit window blinking) light from the tower's rules; with all four,
	// a_sink18 frees the prisoners
	void Mission6(MessageCode code, TPinballComponent* cmp)
	{
		auto end = [&](MessageCode why)
		{
			switch (g_5fea)
			{
			case 0:
				Send(v_lite48, MessageCode::TLightResetAndTurnOff, -1.0f);
				break;
			case 1:
				Send(v_lite48, MessageCode::TLightResetAndTurnOn, -1.0f);
				break;
			default:
				Send(v_lite48, MessageCode::TLightTurnOn, -1.0f);
				Send(v_lite48, MessageCode::TLightFlasherStartTimed, -1.0f);
				break;
			}
			ClearLettersAndLocks();
			Send(prison_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(prison_window_light_group, FT1LightResetOverride, 0.0f);
			if (why == MessageCode::ControlCollision)
				SetState(2);
			CompleteQuest();
			bool keep = InfoField() == 6 || InfoField() == 13;
			if (keep)
				return;
			Send(v_lite47, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_dvtraa, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrab, MessageCode::TGateDisable, 0.0f);
			Send(v_lite125, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite126, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(a_dvtrda, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrdb, MessageCode::TGateDisable, 0.0f);
		};
		if (code == FT1Event79)
		{
			g_5fea = Flashing(v_lite48) ? 2 : LightOn(v_lite48) ? 1 : 0;
			Send(light_sweeps, FT1LightSweepStart, 2.0f);
			Send(v_lite47, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(a_dvtraa, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrab, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrba, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrbb, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrca, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrcb, MessageCode::TGateEnable, 0.0f);
			Send(v_lite125, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(v_lite126, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_dvtrda, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrdb, MessageCode::TGateEnable, 0.0f);
			ScorePoints(nullptr, 100000);
			InfoClear();
			Display(ft1_strings::Get(3033), -1.0f, 0);
			Play(tabc50wave);
			PlayMusic(MusicTrack4);
			Send(v_lite48, MessageCode::TLightFlasherStartTimed, -1.0f);
			return;
		}
		if (code == MessageCode::ControlCollision)
		{
			if (cmp == a_sink18.GetComponent() &&
				Send(prison_window_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) == 4)
			{
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
				Send(prison_window_light_group, FT1GroupRestartNotify, -1.0f);
				end(code);
			}
			return;
		}
		if (code == FT1Event83 || code == FT1Event82)
		{
			if (code == FT1Event83)
			{
				SetInfoField(2);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			}
			end(code);
			return;
		}
		if (code == FT1Event81)
		{
			Play(tabc50wave);
			Send(prison_window_light_group, FT1LightResetOverride, 0.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 15.0f);
			Send(prison_window_light_group, FT1GroupRestartNotify, 0.2f);
			if (TextQueued())
				return;
		}
		if (code == FT1Event81 || code == FT1Event80)
		{
			Countdown(3032);
			return;
		}
		if (code == MessageCode::ControlNotifyTimerExpired && cmp == prison_window_light_group.GetComponent())
		{
			// the chase: the window in the group's MessageField back from its override, the next one blinks when
			// unlit (override on) or holds when lit (override off)
			auto group = prison_window_light_group.Component;
			Send(prison_window_light_group, FT1GroupRestartNotify, 0.2f);
			auto index = group->MessageField;
			auto light = [&](int i) -> TLight*
			{
				return i >= 0 && i < static_cast<int>(group->List.size()) ? group->List[i] : nullptr;
			};
			if (auto previous = light(index))
				previous->Message(FT1LightResetOverride, 0.0f);
			index = index < 3 ? index + 1 : 0;
			group->MessageField = index;
			if (auto next = light(index))
				next->Message(next->light_on() ? FT1LightOverrideOff : FT1LightOverrideOn, 0.0f);
			return;
		}
		if (code == MessageCode::ControlTimerExpired && cmp)
		{
			if (cmp == light_sweeps.GetComponent())
			{
				ResetTowerAndLetters();
				Send(v_lite48, MessageCode::TLightFlasherStartTimed, -1.0f);
				Send(v_lite48, FT1LightResetOverride, 0.0f);
				Send(watch_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(gate_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(prison_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(prison_window_light_group, FT1GroupRestartNotify, 0.2f);
				ReleaseTo(a_sink18.GetComponent(), 0.0f);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.45f);
				MissionEvent(FT1Event80, nullptr);
			}
			else if (cmp == mode_timer.GetComponent())
			{
				Send(prison_window_light_group, FT1LightOverrideOff, 0.0f);
				Send(prison_window_light_group, FT1GroupRestartNotify, -1.0f);
				Field(mode_timer) = InfoField();
				SetState(3);
			}
		}
	}

	// 23:978f - state 7, the gargoyles (text 3036, music tabc5): both rock blockers drop and their spot targets go down;
	// v_lite45/46 flash; the gargoyle spots light from their rules; text 3035 counts down
	void Mission7(MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event79)
		{
			Send(light_sweeps, FT1LightSweepStart, 3.0f);
			Send(gargoyle_spot_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			if (InfoField() == 2)
				Send(v_lite46, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(a_rockb, MessageCode::TBlockerDisable, 0.0f);
			Send(a_spot14, MessageCode::TPopupTargetDisable, 0.0f);
			Send(v_lite45, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(a_rocka, MessageCode::TBlockerDisable, 0.0f);
			Send(a_spot10, MessageCode::TPopupTargetDisable, 0.0f);
			ScorePoints(nullptr, 100000);
			InfoClear();
			Display(ft1_strings::Get(3036), -1.0f, 0);
			Send(v_lite46, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite45, MessageCode::TLightFlasherStartTimed, -1.0f);
			Play(tabc53wave);
			PlayMusic(MusicTrack5);
			return;
		}
		if (code == FT1Event83 || code == FT1Event82)
		{
			if (code == FT1Event83)
			{
				SetInfoField(2);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			}
			ClearLettersAndLocks();
			Send(gargoyle_spot_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(gargoyle_spot_light_group, FT1GroupReset, 0.0f);
			Send(gargoyle_spot_light_group, FT1LightResetOverride, 0.0f);
			Send(v_lite46, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_rockb, MessageCode::TBlockerEnable, 0.0f);
			Send(v_lite45, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_rocka, MessageCode::TBlockerEnable, 0.0f);
			CompleteQuest();
			return;
		}
		if (code == FT1Event81)
		{
			Send(v_lite46, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite45, MessageCode::TLightFlasherStartTimed, -1.0f);
			Play(tabc53wave);
			Send(gargoyle_spot_light_group, FT1LightResetOverride, 0.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 15.0f);
			if (TextQueued())
				return;
		}
		if (code == FT1Event81 || code == FT1Event80)
		{
			Countdown(3035);
			return;
		}
		if (code == MessageCode::ControlTimerExpired && cmp)
		{
			if (cmp == light_sweeps.GetComponent())
			{
				ResetTowerAndLetters();
				Send(v_lite14, MessageCode::TLightResetAndTurnOn, 0.0f);
				Send(gargoyle_spot_light_group, FT1GroupAnimationBackward, 0.1f);
				ReleaseTo(a_sink18.GetComponent(), 0.0f);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.65f);
				MissionEvent(FT1Event80, nullptr);
			}
			else if (cmp == mode_timer.GetComponent())
			{
				Send(v_lite46, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(v_lite45, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(gargoyle_spot_light_group, FT1LightOverrideOff, 0.0f);
				Field(mode_timer) = InfoField();
				SetState(3);
			}
		}
	}

	// 23:9c76 - state 8, the well and the spell (text 3038, music tabc6): the well and spell lights flash, their rules
	// count the shots; the ball comes out of sink 4; text 3037 counts down
	void Mission8(MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event79)
		{
			Send(light_sweeps, FT1LightSweepStart, 4.0f);
			ScorePoints(nullptr, 100000);
			InfoClear();
			Display(ft1_strings::Get(3038), -1.0f, 0);
			Send(well_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(spell_spot_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Play(tabc55wave);
			PlayMusic(MusicTrack6);
			return;
		}
		if (code == FT1Event83 || code == FT1Event82)
		{
			if (code == FT1Event83)
			{
				SetInfoField(2);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			}
			ClearLettersAndLocks();
			CompleteQuest();
			return;
		}
		if (code == FT1Event81)
		{
			Play(tabc55wave);
			Send(well_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(spell_spot_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 15.0f);
			if (TextQueued())
				return;
		}
		if (code == FT1Event81 || code == FT1Event80)
		{
			Countdown(3037);
			return;
		}
		if (code == MessageCode::ControlTimerExpired && cmp)
		{
			if (cmp == light_sweeps.GetComponent())
			{
				ResetTowerAndLetters();
				ReleaseTo(a_sink4.GetComponent(), 0.0f);
				Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.69f);
				MissionEvent(FT1Event80, nullptr);
			}
			else if (cmp == mode_timer.GetComponent())
			{
				Send(well_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(spell_spot_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
				Field(mode_timer) = InfoField();
				Send(well_light_group, FT1LightOverrideOff, 0.0f);
				SetState(3);
			}
		}
	}

	// FT1 84 / 85: a ramp (a_ramp107) has the ball come onto it / leave by its second line (the battle's timer waits
	// while the ball is on it)
	const MessageCode FT1Code84 = MessageCode::Ft1RampEnter;
	const MessageCode FT1Code85 = MessageCode::Ft1RampExit2;

	int g_5ff0 = 0; // the battle's light sweep is running
	int g_5fee = 0; // the ball is on a_ramp107
	int g_5fec = 0; // the battle's seconds: 30 + 5 per weapon and armor piece

	// ds:0x352e - the dragon's fire, from its mouth down the table, indexed by v_lite113's MessageField
	component_tag<TLight>* FireLights[] = {&v_lite113, &v_lite114, &v_lite115, &v_lite58, &v_lite59,
	                                       &v_lite60, &v_lite61, &v_lite62};

	// 23:9fae - state 9, the dragon (text 3048, wave tabc57): after the light sweep the dragon wakes and the jaw opens;
	// the battle lasts g_5fec seconds (the timer only starts at s_oneway2 while the ball saver runs). The weapons
	// (kickback, center post, gate blocker) and armor lit when it starts add time and come up; each lit multiplier
	// light (33-36), v_lite28 and v_lite29 take a hit off the dragon's eight (v_lite114's MessageField, counted down
	// by a_sink19's rule). a_sink19 with the last hit slays it: the mouth shows its pictures, then the super jackpot,
	// 1,500,000, the jackpot, multiplier x5, all weapons and armor. Bumpers, targets, kickouts and sinks 1 and 2 add to
	// the super jackpot meanwhile.
	void Mission9(MessageCode code, TPinballComponent* cmp)
	{
		auto t = control::TableG;
		switch (code)
		{
		case FT1Event79:
		{
			g_5ff0 = 1;
			Send(light_sweeps, FT1LightSweepStart, 5.0f);
			SetActive(a_sink18, false);
			SetActive(a_sink17, false);
			g_5fee = 0;
			g_5fec = 30;
			if (LightOn(v_lite37))
			{
				g_5fec += 5;
				EnableKickback(false);
				Play(tabc24wave);
			}
			if (LightOn(v_lite38))
			{
				g_5fec += 5;
				CenterPostRule(MessageCode::TBlockerEnable, a_center_post.GetComponent());
			}
			if (LightOn(v_lite39))
			{
				g_5fec += 5;
				BloceRule(MessageCode::TBlockerEnable, a_bloce.GetComponent());
			}
			if (LightOn(v_lite31))
				g_5fec += 5;
			if (LightOn(v_lite32))
				g_5fec += 5;
			Field(v_lite114) = 8;
			for (auto light : {&v_lite33, &v_lite34, &v_lite35, &v_lite36, &v_lite28, &v_lite29})
				if (LightOn(*light))
					Field(v_lite114)--;
			bool keep = InfoField() == 6 || InfoField() == 13;
			if (!keep)
			{
				Send(v_lite47, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(a_dvtraa, MessageCode::TGateEnable, 0.0f);
				Send(a_dvtrab, MessageCode::TGateDisable, 0.0f);
			}
			Send(a_dvtrba, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrbb, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrca, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrcb, MessageCode::TGateEnable, 0.0f);
			if (!(InfoField() == 6 || InfoField() == 13))
			{
				Send(v_lite125, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(v_lite126, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
				Send(a_dvtrda, MessageCode::TGateEnable, 0.0f);
				Send(a_dvtrdb, MessageCode::TGateDisable, 0.0f);
			}
			ScorePoints(nullptr, 500000);
			Display(ft1_strings::Get(3048), -1.0f, 0);
			Send(v_lite29, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(v_lite30, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			SetActive(a_kout3, true);
			Play(tabc57wave);
			return;
		}
		case MessageCode::SetTiltLock: // FT1 1011: the armor goes, then as a drain
			ResetArmor();
			[[fallthrough]];
		case FT1Event83:
			SetInfoField(2);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
			[[fallthrough]];
		case FT1Event82:
			SwordStep(3);
			for (auto light : {&v_lite71, &v_lite72, &v_lite73, &v_lite74, &v_lite75})
				Send(*light, MessageCode::TLightResetAndTurnOff, 0.0f);
			ClearLettersAndLocks();
			DragonSleeps(true);
			Send(v_lite77, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite78, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite79, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite76, MessageCode::TLightResetAndTurnOn, 0.0f);
			CompleteQuest();
			return;
		case FT1Code85:
			if (cmp == a_ramp107.GetComponent())
				g_5fee = 0;
			return;
		case FT1Code84:
			if (cmp == a_ramp107.GetComponent())
				g_5fee = 1;
			return;
		case FT1Event81:
			Play(tabc57wave);
			Send(mode_timer, MessageCode::Ft1BlockerTimer, static_cast<float>(g_5fec) / 2.0f);
			Send(v_lite29, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(v_lite30, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			SetActive(a_kout3, true);
			DragonWakes();
			if (TextQueued())
				return;
			[[fallthrough]];
		case FT1Event80:
		{
			auto left = TimeLeftMs(mode_timer);
			if (left)
				Display(Format(3047, left / 1000).c_str(), 0.5f, 0);
			return;
		}
		case MessageCode::ControlCollision: // FT1 74, by the part's type
			if (!cmp)
				return;
			if (dynamic_cast<TBumper*>(cmp)) // 1005
				AddSuperJackpot(50000);
			else if (dynamic_cast<TSoloTarget*>(cmp)) // 1019
				AddSuperJackpot(100000);
			else if (dynamic_cast<TKickout*>(cmp)) // 1012
				AddSuperJackpot(750000);
			else if (dynamic_cast<TSink*>(cmp)) // 1017
			{
				if (cmp == a_sink19.GetComponent())
				{
					if (Field(v_lite114) == 1) // the last hit: the dragon is slain
					{
						if (t->LightGroup)
							t->LightGroup->Message(FT1LightOverrideOn, 0.0f);
						DragonSleeps(false);
						Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
						Field(dragon_mouth_light_group) = 0;
						Send(dragon_mouth_light_group, FT1GroupRestartNotify, 0.0001f);
						InfoClear();
					}
					g_5fee = 0;
				}
				else if (cmp == a_sink1.GetComponent() || cmp == a_sink2.GetComponent())
					AddSuperJackpot(500000);
			}
			else if (dynamic_cast<TOneway*>(cmp)) // 1016
			{
				if (!g_5ff0 && cmp == s_oneway2.GetComponent() && g_5fe0 == 3)
				{
					Send(mode_timer, MessageCode::Ft1BlockerTimer, static_cast<float>(g_5fec));
					Mission9(FT1Event80, nullptr);
				}
			}
			return;
		case MessageCode::ControlNotifyTimerExpired: // FT1 71
			if (cmp == dragon_fire_light_group.GetComponent())
			{
				// the fire runs down the eight lights and back
				auto& index = Field(v_lite113);
				auto fire = [&](MessageCode lightCode)
				{
					if (index >= 0 && index < 8)
						Send(*FireLights[index], lightCode, 0.0f);
				};
				if (g_226e > 0)
				{
					index++;
					if (index == 0)
					{
						Play(tabc58wave);
						fire(MessageCode::TLightResetAndTurnOn);
					}
					else if (index >= 1 && index <= 7)
						fire(MessageCode::TLightResetAndTurnOn);
					else if (index == 8)
						g_226e = -1;
				}
				else
				{
					index--;
					if (index == -1)
						g_226e = 1;
					else
						fire(MessageCode::TLightResetAndTurnOff);
				}
				Send(dragon_fire_light_group, FT1GroupRestartNotify, index >= 6 && index <= 8 ? 0.012f : 0.123f);
			}
			else if (cmp == dragon_mouth_light_group.GetComponent())
			{
				// the slain dragon's pictures (picture font ',' '-' '.' '/' '0'..'3', then none), 0.4 s each
				auto step = Field(dragon_mouth_light_group)++;
				if (step >= 0 && step <= 8)
				{
					static const char* pictures[] = {",", "-", ".", "/", "0", "1", "2", "3", ""};
					Display(pictures[step], -1.0f, 1);
					Send(dragon_mouth_light_group, FT1GroupRestartNotify, 0.4f);
				}
				else if (step == 9)
				{
					if (t->LightGroup)
						t->LightGroup->Message(FT1LightResetOverride, 0.0f);
					auto added = SpecialAddScore(cmp, SuperJackpot);
					Display(Format(3050, added).c_str(), -1.0f, 0);
					ScorePoints(cmp, 1500000);
					CollectJackpot(0.0f, false);
					for (auto light : {&v_lite33, &v_lite34, &v_lite35, &v_lite36})
						Send(*light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					Multiplier = 5;
					EnableKickback(false);
					CenterPostRule(MessageCode::TBlockerEnable, a_center_post.GetComponent());
					BloceRule(MessageCode::TBlockerEnable, a_bloce.GetComponent());
					Send(v_lite28, MessageCode::TLightTurnOnTimed, 30.0f);
					Send(v_lite29, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					Send(v_lite30, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					SetActive(a_kout3, true);
					Send(v_lite31, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					BlocfRule(MessageCode::TBlockerDisable, a_blocf.GetComponent());
					Mission9(FT1Event82, nullptr);
					Send(a_sink16, MessageCode::TSinkResetTimer, -1.0f);
					Send(dragon_letter_light_group, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					AllGems();
				}
			}
			return;
		case MessageCode::Ft1BlockerArrived: // FT1 61: the jaw is open, the fire starts
			if (g_2274 && cmp == a_jaw.GetComponent())
			{
				SetActive(a_ramp107, true);
				SetActive(s_bwal170, true);
				Send(dragon_fire_light_group, FT1GroupRestartNotify, 0.123f);
			}
			return;
		case MessageCode::ControlTimerExpired: // FT1 70
			if (cmp == light_sweeps.GetComponent())
			{
				g_5ff0 = 0;
				ResetTowerAndLetters();
				DragonWakes();
				if (g_5fe0 == 3)
					return; // the timer starts at s_oneway2
				Send(mode_timer, MessageCode::Ft1BlockerTimer, static_cast<float>(g_5fec));
				MissionEvent(FT1Event80, nullptr);
			}
			else if (cmp == mode_timer.GetComponent())
			{
				if (g_5fee)
				{
					Send(mode_timer, MessageCode::Ft1BlockerTimer, 0.05f);
					return;
				}
				Field(mode_timer) = InfoField();
				DragonSleeps(false);
				SetState(3);
			}
			return;
		default:
			return;
		}
	}

	int g_5ff2 = 0; // the egg award on offer (0 none, 1 quest, 4 armor, 5 multiplier, 6 weapon, 7 v_lite57, 8 extra ball)
	int g_5ff4 = 0; // the armor piece on offer

	// 23:ae65 - state 12, s_hole118 has the ball: the dragon eggs lit pick an award, lower when it is already had
	// (8 eggs extra ball, 7 v_lite57, 6 a weapon, 5 the multiplier, 4 armor, 1-3 the next quest; texts 3052-3061).
	// The left flipper takes it (the eggs it cost go out), the right one 25,000; then the hole lets the ball go
	void Mission12(MessageCode code, TPinballComponent* cmp)
	{
		if (code == FT1Event79)
		{
			Field(s_hole118) = 0;
			InfoClear();
			g_5ff2 = Send(dragon_egg_light_group, MessageCode::TLightGroupGetOnCount, 0.0f);
			switch (g_5ff2)
			{
			case 8:
				if (!control::TableG->ExtraBalls)
					break;
				g_5ff2 = 7;
				[[fallthrough]];
			case 7:
				if (!LightOn(v_lite57))
					break;
				g_5ff2 = 6;
				[[fallthrough]];
			case 6:
				if (!(LightOn(v_lite37) && LightOn(v_lite38) && LightOn(v_lite39)))
					break;
				g_5ff2 = 5;
				[[fallthrough]];
			case 5:
				if (!(LightOn(v_lite33) && LightOn(v_lite34) && LightOn(v_lite35) && LightOn(v_lite36)))
					break;
				g_5ff2 = 4;
				[[fallthrough]];
			case 4:
				g_5ff4 = RandomArmor();
				if (g_5ff4 != -1)
					break;
				[[fallthrough]];
			case 1:
			case 2:
			case 3:
				g_5ff2 = 1;
				break;
			default:
				g_5ff2 = 0;
				break;
			}
		}
		if (code == FT1Event79 || code == FT1Event80)
		{
			int text = 0;
			switch (g_5ff2)
			{
			case 0:
				Play(tabc32wave);
				ScorePoints(nullptr, 25000);
				Field(s_hole118) = 0;
				Display(ft1_strings::Get(3068), 2.0f, 0);
				Send(s_hole118, MessageCode::Ft1HoleRelease, 2.0f);
				g_2270 = 1;
				SetInfoField(2);
				MissionEvent(FT1Event79, nullptr);
				return;
			case 1:
			case 2:
			case 3:
				if (Field(a_kout2) >= 4 && Field(a_kout2) <= 8)
					text = 3057 + Field(a_kout2) - 4; // the quest's name
				break;
			case 4:
				text = 3056;
				break;
			case 5:
				text = 3055;
				break;
			case 6:
				text = 3054;
				break;
			case 7:
				text = 3053;
				break;
			default:
				text = 3052;
				break;
			}
			if (text)
				Display(ft1_strings::Get(text), -1.0f, 0);
			if (code == FT1Event79)
				Play(tabc33wave);
			return;
		}
		if (code == MessageCode::TFlipperExtend) // FT1 1 from a flipper
		{
			SetInfoField(2);
			InfoClear();
			if (cmp == a_flip1.GetComponent())
			{
				switch (g_5ff2)
				{
				case 7:
					Play(tabc35wave);
					Send(v_lite57, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
					break;
				case 6:
					RandomWeapon();
					break;
				case 5:
					RaiseMultiplier();
					break;
				case 4:
					AwardArmor(g_5ff4);
					break;
				case 1:
				case 2:
				case 3:
					g_2270 = 1;
					if (QuestRunningStart())
					{
						Send(dragon_egg_light_group, MessageCode::TLightGroupResetAndTurnOff, 1.0f);
						return;
					}
					g_2270 = 0;
					break;
				default:
					AddExtraBall(2.0f);
					break;
				}
				while (g_5ff2-- != 0) // the eggs it cost go out
					Send(dragon_egg_light_group, MessageCode::TLightGroupResetAndTurnOff, 1.0f);
				Field(s_hole118) = 1;
			}
			else
			{
				ScorePoints(nullptr, 25000);
				Field(s_hole118) = 0;
			}
			Send(s_hole118, MessageCode::Ft1HoleRelease, 0.0f);
			g_2270 = 1;
			MissionEvent(FT1Event79, nullptr);
		}
	}

	// 23:b47c - state 13, multiball (text 3051): the tower's diverters set, the windows flash, three more balls out of
	// sinks 18, 17 and 16 (1, 2, 3 s, their lights 48-50 flash), v_lite78 flashes. When the last ball but one drains
	// it ends: the windows off, v_lite78 on, normal play (state 2), the diverters back unless a quest uses them
	void Mission13(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		if (code == FT1Event79)
		{
			ResetTowerAndLetters();
			Send(v_lite47, MessageCode::TLightResetAndTurnOn, 0.0f);
			Send(a_dvtraa, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrab, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrba, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrbb, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrca, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrcb, MessageCode::TGateDisable, 0.0f);
			Send(v_lite125, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(v_lite126, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(a_dvtrda, MessageCode::TGateDisable, 0.0f);
			Send(a_dvtrdb, MessageCode::TGateEnable, 0.0f);
			Send(prison_window_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(watch_window_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(gate_window_light_group, MessageCode::TLightFlasherStartTimed, -1.0f);
			t->MultiballCount += 3; // +0x146
			Send(a_sink18, MessageCode::TSinkResetTimer, 1.0f);
			Send(v_lite48, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Send(a_sink17, MessageCode::TSinkResetTimer, 2.0f);
			Send(v_lite49, MessageCode::TLightFlasherStartTimedThenStayOff, 2.0f);
			Send(a_sink16, MessageCode::TSinkResetTimer, 3.0f);
			Send(v_lite50, MessageCode::TLightFlasherStartTimedThenStayOff, 3.0f);
			Send(v_lite78, MessageCode::TLightFlasherStartTimed, -1.0f);
			InfoClear();
		}
		if (code == FT1Event79 || code == FT1Event80)
		{
			Display(ft1_strings::Get(3051), -1.0f, 0);
			return;
		}
		if (code == FT1Event83 && t->MultiballCount == 1)
		{
			ClearLettersAndLocks();
			Send(prison_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(watch_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(gate_window_light_group, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite78, MessageCode::TLightResetAndTurnOn, 0.0f);
			SetInfoField(2);
			MissionEvent(FT1Event79, nullptr);
			if (!(InfoField() == 6 || InfoField() == 13))
			{
				Send(v_lite47, MessageCode::TLightResetAndTurnOff, 0.0f);
				Send(a_dvtraa, MessageCode::TGateEnable, 0.0f);
				Send(a_dvtrab, MessageCode::TGateDisable, 0.0f);
			}
			if (InfoField() == 6 || InfoField() == 13)
				return;
			Send(v_lite125, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite126, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
			Send(a_dvtrda, MessageCode::TGateEnable, 0.0f);
			Send(a_dvtrdb, MessageCode::TGateDisable, 0.0f);
		}
	}

	// 23:b89a - state 14, two-ball multiball (text 3062, music tabc1): one more ball out of sink 1 and the ball in
	// a_kout2 (unless it went to s_hole118), lights 65/66 flash; it ends when one ball is left
	void Mission14(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		if (code == FT1Event79)
		{
			ResetTowerAndLetters();
			t->MultiballCount++;
			Send(a_sink1, MessageCode::TSinkResetTimer, -1.0f);
			if (!g_2270)
				Send(a_kout2, MessageCode::TKickoutRestartTimer, -1.0f); // FT1 64
			Send(v_lite66, MessageCode::TLightFlasherStartTimed, 1.0f);
			Send(v_lite65, MessageCode::TLightFlasherStartTimed, 1.0f);
			ScorePoints(nullptr, 100000);
			Send(v_lite77, MessageCode::TLightFlasherStartTimedThenStayOn, -1.0f);
			InfoClear();
			Play(tabc48wave);
			PlayMusic(MusicTrack1);
		}
		if (code == FT1Event79 || code == FT1Event80)
		{
			Display(ft1_strings::Get(3062), -1.0f, 0);
			return;
		}
		if (code == FT1Event83 && t->MultiballCount == 1)
		{
			ClearLettersAndLocks();
			Send(v_lite77, MessageCode::TLightResetAndTurnOn, 0.0f);
			SetInfoField(2);
			MissionEvent(FT1Event79, nullptr);
			Send(v_lite63, MessageCode::TLightResetAndTurnOff, 0.0f);
			Send(v_lite64, MessageCode::TLightResetAndTurnOff, 0.0f);
		}
	}

	// 23:ba66 - state 15, all six gems: five-ball multiball (text 3063, wave tabc61): balls out of the plunger (unless
	// one went to s_hole118), sinks 1, 2, 4 and 18; the gems flash; it ends when one ball is left
	void Mission15(MessageCode code, TPinballComponent*)
	{
		auto t = control::TableG;
		component_tag<TLight>* gems[] = {&v_lite22, &v_lite23, &v_lite24, &v_lite25, &v_lite26, &v_lite27};
		if (code == FT1Event79)
		{
			ResetTowerAndLetters();
			t->MultiballCount += 4;
			if (g_2270)
			{
				t->MultiballCount--;
				Send(s_plunger1, MessageCode::PlungerRelaunchBall, 0.96f); // FT1 1019
			}
			Send(a_sink1, MessageCode::TSinkResetTimer, -1.0f);
			Send(v_lite66, MessageCode::TLightFlasherStartTimed, 1.0f);
			Send(a_sink2, MessageCode::TSinkResetTimer, -1.0f);
			Send(v_lite65, MessageCode::TLightFlasherStartTimed, 1.0f);
			Send(a_sink4, MessageCode::TSinkResetTimer, 1.0f);
			Send(well_light_group, MessageCode::TLightFlasherStartTimed, 1.0f);
			Send(a_sink18, MessageCode::TSinkResetTimer, 1.0f);
			Send(v_lite48, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Send(dragon_letter_light_group, FT1LightResetOverride, 0.0f);
			for (auto gem : gems)
				Send(*gem, MessageCode::TLightTurnOn, 0.0f);
			for (auto gem : gems)
				Send(*gem, MessageCode::TLightFlasherStartTimed, -1.0f);
			Send(v_lite79, MessageCode::TLightFlasherStartTimedThenStayOn, -1.0f);
			InfoClear();
			Play(tabc61wave);
		}
		if (code == FT1Event79 || code == FT1Event80)
		{
			Display(ft1_strings::Get(3063), -1.0f, 0);
			return;
		}
		if (code == FT1Event83 && t->MultiballCount == 1)
		{
			ClearLettersAndLocks();
			Send(v_lite79, MessageCode::TLightResetAndTurnOn, 0.0f);
			for (auto gem : gems)
				Send(*gem, MessageCode::TLightResetAndTurnOff, 0.0f);
			SetInfoField(2);
			MissionEvent(FT1Event79, nullptr);
		}
	}

	// 23:c4c7 - state 0, the game is over: the music back, the table's game over (FT1 1024); when a score makes the
	// high score table (14:0505) all the lights flash 3 s and text 3065 shows
	void Mission0(MessageCode code, TPinballComponent*)
	{
		if (code != FT1Event79)
			return;
		if (MusicPlaying != MusicTrack1)
			PlayMusic(MusicTrack1);
		auto t = control::TableG;
		t->Message(MessageCode::GameOver, 0.0f); // FT1 1024 to the table
		bool high = false;
		for (int i = t->PlayerCount - 1; i >= 0 && !high; i--)
		{
			auto score = i == t->CurrentPlayer ? t->CurScore : t->PlayerScores[i].Score;
			high = high_score::get_score_position(score) >= 0;
		}
		if (!high)
			return;
		Play(tabc56wave);
		if (t->LightGroup)
			t->LightGroup->Message(MessageCode::TLightFlasherStartTimedThenStayOff, 3.0f);
		Display(ft1_strings::Get(3065), -1.0f, 0);
	}

	int g_5ff6 = 0;  // the bonus count is hurried (a flipper)
	int g_5ff8 = 0;  // the bonus count's step
	int g_5ffa = 0;  // the bonus so far

	// 23:be51 - state 11, the end of the ball: the lights go out and the bonus is counted, one line per 2 s (a flipper
	// hurries it; nothing when tilted): gems x2,000 (3012), loops x100,000 (3013), weapons x50,000 (3014), armor
	// x75,000 (3015), eggs x150,000 (3069), multiplier x75,000 (3070), the held bonus (3016); the total is scored
	// (3017) and kept when v_lite31 (bonus hold) is lit. Then an extra ball, the next player, or the game is over.
	// mode_timer (30 s) ends it if it hangs.
	void Mission11(MessageCode code, TPinballComponent* cmp)
	{
		auto t = control::TableG;
		auto show = [&](int textId, int value, component_tag<TSound>& sound)
		{
			Display(Format(textId, value).c_str(), 2.0f, 0);
			Play(sound);
		};
		switch (code)
		{
		case MessageCode::TFlipperExtend: // FT1 1
			g_5ff6 = 1;
			InfoClear();
			Mission11(FT1Event80, cmp);
			return;
		case FT1Event83:
			if (g_5ff8 >= 8)
				break;
			return;
		case MessageCode::ControlTimerExpired: // FT1 70
			if (cmp != mode_timer.GetComponent())
				return;
			g_2276 = 0;
			g_5ff8 = 8;
			Mission11(FT1Event80, nullptr);
			return;
		case FT1Event79:
			g_5ff6 = 0;
			g_5ff8 = 0;
			g_5ffa = 0;
			Send(mode_timer, MessageCode::Ft1BlockerTimer, 30.0f);
			if (t->TiltLockFlag && g_2276 != 1)
				g_5ff8 = 8;
			if (t->LightGroup)
				t->LightGroup->Message(FT1LightOverrideOff, 0.0f);
			InfoClear();
			Play(tabc44wave);
			break;
		case FT1Event80:
			break;
		default:
			return;
		}
		// event 80: the next step
		for (;;)
		{
			auto step = g_5ff8++;
			int value = 0, text = 0;
			switch (step)
			{
			case 0:
				value = g_5fe2 * 2000;
				text = 3012;
				break;
			case 1:
			{
				int loops = Field(v_lite28);
				ClearQuestLight();
				value = loops * 100000;
				text = 3013;
				break;
			}
			case 2:
			{
				int weapons = (LightOn(v_lite37) ? 1 : 0) + (LightOn(v_lite38) ? 1 : 0) + (LightOn(v_lite39) ? 1 : 0);
				value = weapons * 50000;
				text = 3014;
				break;
			}
			case 3:
				value = g_5fe4 * 75000;
				text = 3015;
				break;
			case 4:
				value = Send(dragon_egg_light_group, MessageCode::TLightGroupGetOnCount, 0.0f) * 150000;
				text = 3069;
				break;
			case 5:
				if (Multiplier > 1)
					value = Multiplier * 75000;
				text = 3070;
				break;
			case 6:
				value = Field(drain); // the held bonus
				text = 3016;
				break;
			case 7:
				if (LightOn(v_lite31))
				{
					Send(v_lite31, MessageCode::TLightResetAndTurnOff, 0.0f);
					Field(drain) = g_5ffa;
				}
				else
					Field(drain) = 0;
				if (!g_5ffa)
					continue;
				{
					auto added = SpecialAddScore(cmp, g_5ffa);
					if (g_5ff6)
						continue;
					show(3017, added, tabc13wave);
				}
				return;
			default:
			{
				// the ball is over
				if (g_2276 && t->MultiballCount)
					return;
				g_2276 = 0;
				Send(mode_timer, MessageCode::Ft1BlockerTimer, -1.0f);
				if (t->ExtraBalls)
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
						SetInfoField(0);
						MissionEvent(FT1Event79, nullptr);
						if (t->LightGroup)
							t->LightGroup->Message(FT1LightResetOverride, 0.0f);
						return;
					}
					t->Message(MessageCode::ClearTiltLock, 0.0f);    // FT1 1012
					Send(drain, MessageCode::Reset, 0.0f);             // FT1 1011 to the drain
					t->Message(MessageCode::SwitchToNextPlayer, 0.0f); // FT1 1023
				}
				Send(s_plunger1, MessageCode::PlungerStartFeedTimer, 0.96f); // FT1 1017
				if (t->LightGroup)
					t->LightGroup->Message(FT1LightResetOverride, 0.0f);
				return;
			}
			}
			if (!value)
				continue;
			g_5ffa += value;
			if (g_5ff6)
				continue;
			show(text, value, tabc06wave);
			return;
		}
	}

	// 23:c5a9 - any other state: back to normal play
	void MissionOther(MessageCode, TPinballComponent*)
	{
		SetInfoField(2);
		MissionEvent(FT1Event79, nullptr);
	}


	// 23:2b8a - the bumpers: 74 = hit (in the gem quest only the lights and the count): tabc03, bumpers 3 and 4 flash
	// v_lite65 / v_lite66, the bumper's score; every tenth hit (a_bump1's MessageField) the next multiplier
	void BumperRule(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		if (InfoField() != 4)
			Play(tabc03wave);
		if (caller == a_bump3.GetComponent())
			Send(v_lite65, MessageCode::TLightFlasherStartTimed, 0.5f);
		else if (caller == a_bump4.GetComponent())
			Send(v_lite66, MessageCode::TLightFlasherStartTimed, 0.5f);
		if (InfoField() != 4)
			ScoreHit(caller, 0);
		auto count = Field(a_bump1) + 1;
		if (count == 10)
		{
			Field(a_bump1) = 0;
			RaiseMultiplier();
		}
		else
			Field(a_bump1) = count;
	}

	// 23:2ced / 23:2d4c - the rebounders: their light on 0.1 s, their score
	void Rebound1Rule(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Send(v_lite120, MessageCode::TLightTurnOnTimed, 0.1f);
		ScoreHit(caller, 0);
	}

	void Rebound2Rule(MessageCode code, TPinballComponent* caller)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Send(v_lite121, MessageCode::TLightTurnOnTimed, 0.1f);
		ScoreHit(caller, 0);
	}

	// 23:2dab - after a spot of the five (lights 3-7): all five lit go out (flashing 1 s); in normal play gem
	// v_lite23 lights and, unless that was the last gem, an armor piece; otherwise the spot's score and tabc05
	void FiveSpots(TPinballComponent* caller)
	{
		component_tag<TLight>* lights[] = {&v_lite3, &v_lite4, &v_lite5, &v_lite6, &v_lite7};
		bool all = true;
		for (auto light : lights)
			all = all && LightOn(*light);
		if (all)
		{
			for (auto light : lights)
				Send(*light, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			if (InfoField() == 2)
			{
				Send(v_lite23, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
				if (AllGems())
					return;
			}
			AwardRandomArmor();
			return;
		}
		if (InfoField() == 4)
			return;
		ScoreHit(caller, 0);
		Play(tabc05wave);
	}

	// 23:2f95 / 3082 / 316f / 325c / 3349 - spots 4-7 and 22: in the well and spell quest (state 8) a_sink4 keeps
	// which one (0-4) and its text shows (3039-3043); otherwise its light (3-7) lights and FiveSpots
	void SpellSpot(MessageCode code, TPinballComponent* caller, int index, component_tag<TLight>& light)
	{
		if (code != MessageCode::ControlCollision)
			return;
		if (InfoField() == 8)
		{
			Field(a_sink4) = index;
			Play(tabc05wave);
			Display(ft1_strings::Get(3039 + index), 2.0f, 0);
			return;
		}
		Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		FiveSpots(caller);
	}

	void Spot4Rule(MessageCode code, TPinballComponent* caller) { SpellSpot(code, caller, 0, v_lite3); }
	void Spot5Rule(MessageCode code, TPinballComponent* caller) { SpellSpot(code, caller, 1, v_lite4); }
	void Spot6Rule(MessageCode code, TPinballComponent* caller) { SpellSpot(code, caller, 2, v_lite5); }
	void Spot7Rule(MessageCode code, TPinballComponent* caller) { SpellSpot(code, caller, 3, v_lite6); }
	void Spot22Rule(MessageCode code, TPinballComponent* caller) { SpellSpot(code, caller, 4, v_lite7); }

	// 23:3436 - after spots 1-3 (lights 8-10): all three lit go out, the second score; in normal play gem v_lite27
	// lights and, unless that was the last gem, the tower opens (tabc07, text 3003); tabc06 except in the gem quest.
	// Otherwise the spot's score and tabc06
	void ThreeSpots(TPinballComponent* caller)
	{
		if (LightOn(v_lite8) && LightOn(v_lite9) && LightOn(v_lite10))
		{
			Send(v_lite8, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Send(v_lite9, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			Send(v_lite10, MessageCode::TLightFlasherStartTimedThenStayOff, 1.0f);
			ScoreHit(caller, 1);
			if (InfoField() == 2)
			{
				Send(v_lite27, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
				if (!AllGems())
				{
					OpenTower();
					Play(tabc07wave);
					Display(ft1_strings::Get(3003), 2.0f, 0);
				}
			}
			if (InfoField() != 4)
				Play(tabc06wave);
			return;
		}
		if (InfoField() == 4)
			return;
		ScoreHit(caller, 0);
		Play(tabc06wave);
	}

	// 23:35e3 / 3640 / 369d - spots 3, 2, 1: their light (8, 9, 10) lights, then ThreeSpots
	void ThreeSpot(MessageCode code, TPinballComponent* caller, component_tag<TLight>& light)
	{
		if (code != MessageCode::ControlCollision)
			return;
		Send(light, MessageCode::TLightFlasherStartTimedThenStayOn, 1.0f);
		ThreeSpots(caller);
	}

	void Spot3Rule(MessageCode code, TPinballComponent* caller) { ThreeSpot(code, caller, v_lite8); }
	void Spot2Rule(MessageCode code, TPinballComponent* caller) { ThreeSpot(code, caller, v_lite9); }
	void Spot1Rule(MessageCode code, TPinballComponent* caller) { ThreeSpot(code, caller, v_lite10); }

	// ---- rule functions not translated yet (their components do nothing yet) ----
	void Rule_39f7(MessageCode, TPinballComponent*) {} // TODO(23:39f7): a_spot10
	void Rule_3a9f(MessageCode, TPinballComponent*) {} // TODO(23:3a9f): a_spot8
	void Rule_3b77(MessageCode, TPinballComponent*) {} // TODO(23:3b77): a_spot14
	void Rule_3c2e(MessageCode, TPinballComponent*) {} // TODO(23:3c2e): a_spot12
	void Rule_3d06(MessageCode, TPinballComponent*) {} // TODO(23:3d06): a_spot27
	void Rule_3eba(MessageCode, TPinballComponent*) {} // TODO(23:3eba): a_spot19
	void Rule_3f93(MessageCode, TPinballComponent*) {} // TODO(23:3f93): a_spot26
	void Rule_406c(MessageCode, TPinballComponent*) {} // TODO(23:406c): a_spot28
	void Rule_4157(MessageCode, TPinballComponent*) {} // TODO(23:4157): a_spot30
	void Rule_41b6(MessageCode, TPinballComponent*) {} // TODO(23:41b6): a_spot31
	void Rule_4931(MessageCode, TPinballComponent*) {} // TODO(23:4931): a_sink1
	void Rule_4afc(MessageCode, TPinballComponent*) {} // TODO(23:4afc): a_sink2
	void Rule_4c97(MessageCode, TPinballComponent*) {} // TODO(23:4c97): a_sink4
	void Rule_53e7(MessageCode, TPinballComponent*) {} // TODO(23:53e7): a_ramp103
	void Rule_55ee(MessageCode, TPinballComponent*) {} // TODO(23:55ee): a_ramp6
	void Rule_5701(MessageCode, TPinballComponent*) {} // TODO(23:5701): s_oneway5
	void Rule_58d4(MessageCode, TPinballComponent*) {} // TODO(23:58d4): a_accl1
	void Rule_5970(MessageCode, TPinballComponent*) {} // TODO(23:5970): s_trip1, s_trip2, v_lite69
	void Rule_5a62(MessageCode, TPinballComponent*) {} // TODO(23:5a62): a_roll7
	void Rule_5b25(MessageCode, TPinballComponent*) {} // TODO(23:5b25): a_roll5
	void Rule_5b69(MessageCode, TPinballComponent*) {} // TODO(23:5b69): a_roll2
	void Rule_5bad(MessageCode, TPinballComponent*) {} // TODO(23:5bad): a_roll6
	void Rule_5c74(MessageCode, TPinballComponent*) {} // TODO(23:5c74): a_roll1
	void Rule_5d3b(MessageCode, TPinballComponent*) {} // TODO(23:5d3b): s_rexit19
	void Rule_5db7(MessageCode, TPinballComponent*) {} // TODO(23:5db7): s_rexit18
	void Rule_5e33(MessageCode, TPinballComponent*) {} // TODO(23:5e33): a_flip2
	void Rule_5f13(MessageCode, TPinballComponent*) {} // TODO(23:5f13): a_kick1
	void Rule_5fe4(MessageCode, TPinballComponent*) {} // TODO(23:5fe4): a_kout1
	void Rule_6122(MessageCode, TPinballComponent*) {} // TODO(23:6122): a_kout2
	void Rule_6241(MessageCode, TPinballComponent*) {} // TODO(23:6241): s_hole118
	void Rule_6394(MessageCode, TPinballComponent*) {} // TODO(23:6394): a_sink15
	void Rule_658a(MessageCode, TPinballComponent*) {} // TODO(23:658a): a_sink18
	void Rule_678f(MessageCode, TPinballComponent*) {} // TODO(23:678f): a_sink13
	void Rule_68ec(MessageCode, TPinballComponent*) {} // TODO(23:68ec): a_sink17
	void Rule_6a49(MessageCode, TPinballComponent*) {} // TODO(23:6a49): a_sink9
	void Rule_6c5c(MessageCode, TPinballComponent*) {} // TODO(23:6c5c): a_sink10
	void Rule_6ea6(MessageCode, TPinballComponent*) {} // TODO(23:6ea6): v_wlite96, v_wlite97, v_wlite98, v_wlite99
	void Rule_6eea(MessageCode, TPinballComponent*) {} // TODO(23:6eea): s_trip6, s_trip5
	void Rule_71e4(MessageCode, TPinballComponent*) {} // TODO(23:71e4): s_trip3, s_trip4
	void Rule_73e6(MessageCode, TPinballComponent*) {} // TODO(23:73e6): s_trip8, s_trip9
	void Rule_75d3(MessageCode, TPinballComponent*) {} // TODO(23:75d3): a_kout3
	void Rule_7617(MessageCode, TPinballComponent*) {} // TODO(23:7617): a_sink19
	void Rule_109f(MessageCode, TPinballComponent*) {} // TODO(23:109f): a_rocka
	void Rule_10f8(MessageCode, TPinballComponent*) {} // TODO(23:10f8): a_rockb
	void Rule_79db(MessageCode, TPinballComponent*) {} // TODO(23:79db): v_lite37
	void Rule_7a8d(MessageCode, TPinballComponent*) {} // TODO(23:7a8d): v_lite29

	// ds:0x2a34 - the rule table (23:0595 links it): each component's rule function and scores
	const int Scores4[] = {1500};
	const int Scores5[] = {1500};
	const int Scores6[] = {1500};
	const int Scores7[] = {1500};
	const int Scores8[] = {1000};
	const int Scores9[] = {1000};
	const int Scores10[] = {2500};
	const int Scores11[] = {2500};
	const int Scores12[] = {2500};
	const int Scores13[] = {2500};
	const int Scores14[] = {2500};
	const int Scores15[] = {2000, 10000};
	const int Scores16[] = {2000, 10000};
	const int Scores17[] = {2000, 10000};
	const int Scores18[] = {3000, 15000};
	const int Scores19[] = {3000, 15000};
	const int Scores20[] = {3000, 15000};
	const int Scores21[] = {3000, 15000};
	const int Scores22[] = {5000};
	const int Scores23[] = {20000};
	const int Scores24[] = {50000};
	const int Scores26[] = {2500};
	const int Scores27[] = {2500};
	const int Scores29[] = {5000};
	const int Scores30[] = {5000};
	const int Scores34[] = {25000, 75000, 100000, 125000, 150000};
	const int Scores35[] = {75000, 500000};
	const int Scores36[] = {20000, 50000, 100000, 1000000};
	const int Scores37[] = {20000, 50000, 100000, 1000000};
	const int Scores39[] = {15000};
	const int Scores40[] = {30000};
	const int Scores41[] = {30000};
	const int Scores42[] = {10000};
	const int Scores43[] = {10000};
	const int Scores51[] = {100000};
	const int Scores52[] = {100000};
	const int Scores53[] = {200000};
	const int Scores54[] = {200000};
	const int Scores55[] = {300000};
	const int Scores56[] = {300000};
	const int Scores57[] = {1500};
	const int Scores58[] = {1500};
	const int Scores59[] = {1500};
	const int Scores60[] = {1500};
	const int Scores61[] = {25000, 50000, 75000, 100000};
	const int Scores62[] = {25000, 50000, 75000, 100000};
	const int Scores63[] = {40000, 80000, 120000};
	const int Scores64[] = {40000, 80000, 120000};
	const int Scores65[] = {50000, 100000};
	const int Scores66[] = {50000, 100000};
	component_tag<> RuleTags[] = {{"drain"}, {"s_plunger1"}, {"s_oneway2"}, {"v_lite40"}, {"a_bump1"}, {"a_bump2"}, {"a_bump3"}, {"a_bump4"}, {"a_rebo1"}, {"a_rebo2"}, {"a_spot4"}, {"a_spot5"}, {"a_spot6"}, {"a_spot7"}, {"a_spot22"}, {"a_spot3"}, {"a_spot2"}, {"a_spot1"}, {"a_spot10"}, {"a_spot8"}, {"a_spot14"}, {"a_spot12"}, {"a_spot27"}, {"a_spot19"}, {"a_spot26"}, {"a_spot28"}, {"a_spot30"}, {"a_spot31"}, {"a_center_post"}, {"a_sink1"}, {"a_sink2"}, {"a_sink4"}, {"a_ramp103"}, {"a_ramp6"}, {"s_oneway5"}, {"a_accl1"}, {"s_trip1"}, {"s_trip2"}, {"v_lite69"}, {"a_roll7"}, {"a_roll5"}, {"a_roll2"}, {"a_roll6"}, {"a_roll1"}, {"s_rexit19"}, {"s_rexit18"}, {"a_flip2"}, {"a_kick1"}, {"a_kout1"}, {"a_kout2"}, {"s_hole118"}, {"a_sink15"}, {"a_sink18"}, {"a_sink13"}, {"a_sink17"}, {"a_sink9"}, {"a_sink10"}, {"v_wlite96"}, {"v_wlite97"}, {"v_wlite98"}, {"v_wlite99"}, {"s_trip6"}, {"s_trip5"}, {"s_trip3"}, {"s_trip4"}, {"s_trip8"}, {"s_trip9"}, {"a_bloce"}, {"a_blocf"}, {"a_kout3"}, {"a_sink19"}, {"a_rocka"}, {"a_rockb"}, {"v_lite37"}, {"v_lite29"}};
	component_info Rules[] = {
		{RuleTags[0], {DrainRule, 0, nullptr}},
		{RuleTags[1], {PlungerRule, 0, nullptr}},
		{RuleTags[2], {OnewayRule2, 0, nullptr}},
		{RuleTags[3], {LightRule40, 0, nullptr}},
		{RuleTags[4], {BumperRule, 1, Scores4}},
		{RuleTags[5], {BumperRule, 1, Scores5}},
		{RuleTags[6], {BumperRule, 1, Scores6}},
		{RuleTags[7], {BumperRule, 1, Scores7}},
		{RuleTags[8], {Rebound1Rule, 1, Scores8}},
		{RuleTags[9], {Rebound2Rule, 1, Scores9}},
		{RuleTags[10], {Spot4Rule, 1, Scores10}},
		{RuleTags[11], {Spot5Rule, 1, Scores11}},
		{RuleTags[12], {Spot6Rule, 1, Scores12}},
		{RuleTags[13], {Spot7Rule, 1, Scores13}},
		{RuleTags[14], {Spot22Rule, 1, Scores14}},
		{RuleTags[15], {Spot3Rule, 2, Scores15}},
		{RuleTags[16], {Spot2Rule, 2, Scores16}},
		{RuleTags[17], {Spot1Rule, 2, Scores17}},
		{RuleTags[18], {Rule_39f7, 2, Scores18}},
		{RuleTags[19], {Rule_3a9f, 2, Scores19}},
		{RuleTags[20], {Rule_3b77, 2, Scores20}},
		{RuleTags[21], {Rule_3c2e, 2, Scores21}},
		{RuleTags[22], {Rule_3d06, 1, Scores22}},
		{RuleTags[23], {Rule_3eba, 1, Scores23}},
		{RuleTags[24], {Rule_3f93, 1, Scores24}},
		{RuleTags[25], {Rule_406c, 0, nullptr}},
		{RuleTags[26], {Rule_4157, 1, Scores26}},
		{RuleTags[27], {Rule_41b6, 1, Scores27}},
		{RuleTags[28], {CenterPostRule, 0, nullptr}},
		{RuleTags[29], {Rule_4931, 1, Scores29}},
		{RuleTags[30], {Rule_4afc, 1, Scores30}},
		{RuleTags[31], {Rule_4c97, 0, nullptr}},
		{RuleTags[32], {Rule_53e7, 0, nullptr}},
		{RuleTags[33], {Rule_55ee, 0, nullptr}},
		{RuleTags[34], {Rule_5701, 5, Scores34}},
		{RuleTags[35], {Rule_58d4, 2, Scores35}},
		{RuleTags[36], {Rule_5970, 4, Scores36}},
		{RuleTags[37], {Rule_5970, 4, Scores37}},
		{RuleTags[38], {Rule_5970, 0, nullptr}},
		{RuleTags[39], {Rule_5a62, 1, Scores39}},
		{RuleTags[40], {Rule_5b25, 1, Scores40}},
		{RuleTags[41], {Rule_5b69, 1, Scores41}},
		{RuleTags[42], {Rule_5bad, 1, Scores42}},
		{RuleTags[43], {Rule_5c74, 1, Scores43}},
		{RuleTags[44], {Rule_5d3b, 0, nullptr}},
		{RuleTags[45], {Rule_5db7, 0, nullptr}},
		{RuleTags[46], {Rule_5e33, 0, nullptr}},
		{RuleTags[47], {Rule_5f13, 0, nullptr}},
		{RuleTags[48], {Rule_5fe4, 0, nullptr}},
		{RuleTags[49], {Rule_6122, 0, nullptr}},
		{RuleTags[50], {Rule_6241, 0, nullptr}},
		{RuleTags[51], {Rule_6394, 1, Scores51}},
		{RuleTags[52], {Rule_658a, 1, Scores52}},
		{RuleTags[53], {Rule_678f, 1, Scores53}},
		{RuleTags[54], {Rule_68ec, 1, Scores54}},
		{RuleTags[55], {Rule_6a49, 1, Scores55}},
		{RuleTags[56], {Rule_6c5c, 1, Scores56}},
		{RuleTags[57], {Rule_6ea6, 1, Scores57}},
		{RuleTags[58], {Rule_6ea6, 1, Scores58}},
		{RuleTags[59], {Rule_6ea6, 1, Scores59}},
		{RuleTags[60], {Rule_6ea6, 1, Scores60}},
		{RuleTags[61], {Rule_6eea, 4, Scores61}},
		{RuleTags[62], {Rule_6eea, 4, Scores62}},
		{RuleTags[63], {Rule_71e4, 3, Scores63}},
		{RuleTags[64], {Rule_71e4, 3, Scores64}},
		{RuleTags[65], {Rule_73e6, 2, Scores65}},
		{RuleTags[66], {Rule_73e6, 2, Scores66}},
		{RuleTags[67], {BloceRule, 0, nullptr}},
		{RuleTags[68], {BlocfRule, 0, nullptr}},
		{RuleTags[69], {Rule_75d3, 0, nullptr}},
		{RuleTags[70], {Rule_7617, 0, nullptr}},
		{RuleTags[71], {Rule_109f, 0, nullptr}},
		{RuleTags[72], {Rule_10f8, 0, nullptr}},
		{RuleTags[73], {Rule_79db, 0, nullptr}},
		{RuleTags[74], {Rule_7a8d, 0, nullptr}},
	};

	// 23:0657 - the rules' message handler (control::handler): the mission system hears every message first, then the
	// component's own rule runs
	void ControlHandler(MessageCode code, TPinballComponent* cmp)
	{
		MissionEvent(code, cmp);
		if (!cmp || !cmp->Control)
			return;
		// TODO(23:068f): code 1021 (FT1 0x3fd) goes to a virtual of the control object instead
		cmp->Control->ControlFunc(code, cmp);
	}
}

bool ft1_dragon::Wanted()
{
	return pb::DevDebug && getenv("FT1_DRAGON_FULL");
}

bool ft1_dragon::Link(TPinballTable* table)
{
	(void)table;
	Clear();
	std::vector<component_tag_base*> tags(std::begin(AllTags), std::end(AllTags));
	tags.insert(tags.end(), TagRegistry().begin(), TagRegistry().end());
	for (auto tag : tags)
		if (!control::make_component_link(*tag))
			ftlog::Line(std::string("FT1 Dragon's Keep rules: component missing on the table: ") + tag->Name);
	// 23:0595: every component of the rule table gets its rule function and scores
	int linked = 0;
	for (auto& rule : Rules)
		if (auto comp = control::make_component_link(rule.Tag))
		{
			comp->Control = &rule.Control;
			linked++;
		}
		else
			ftlog::Line(std::string("FT1 Dragon's Keep rules: rule component missing: ") + rule.Tag.Name);
	if (auto hole = dynamic_cast<THole*>(s_hole118.GetComponent()))
		hole->Ft1Hold = true; // the FT1 hole keeps its ball until the rules let it go (FT1 67)
	TRamp::Ft1Events = true;
	ftlog::Line("FT1 Dragon's Keep: " + std::to_string(linked) + " components linked to their rules");
	Active = true;
	ftlog::Line("FT1 Dragon's Keep: full translation active (work in progress), " + std::to_string(Translated) +
	            " rule functions");
	return Active;
}

void ft1_dragon::Clear()
{
	Active = false;
	TRamp::Ft1Events = false;
	Translated = 0;
	g_5fdc = nullptr;
	g_5fe4 = 0;
	Multiplier = 1;
	SuperJackpot = 20000;
	SuperJackpotFlag = false;
}

bool ft1_dragon::Handle(MessageCode code, TPinballComponent* cmp)
{
	if (!Active)
		return false;
	if (code == MessageCode::ControlBallCaptured && dynamic_cast<THole*>(cmp))
		code = FT1Code66;
	ControlHandler(code, cmp); // 23:0657: the mission system hears every message, with a rule or without

	return true;
}

bool ft1_dragon::TableHandler(MessageCode code)
{
	if (!Active)
		return false;
	TableControl(code);
	return true;
}

int ft1_dragon::TranslatedCount()
{
	return Translated;
}
