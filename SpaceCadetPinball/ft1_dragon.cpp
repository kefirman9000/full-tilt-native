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
#include "score.h"
#include "TBlocker.h"
#include "TLight.h"
#include "TLightGroup.h"
#include "TPinballComponent.h"
#include "TPinballTable.h"
#include "TSound.h"
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
	component_tag<> a_kick1{"a_kick1"}, a_kout3{"a_kout3"}, a_blocf{"a_blocf"}, a_bloce{"a_bloce"},
	                a_center_post{"a_center_post"};

	component_tag_base* AllTags[] =
	{
		&v_lite28, &v_lite29, &v_lite30, &v_lite31, &v_lite32, &v_lite33, &v_lite34, &v_lite35, &v_lite36, &v_lite37,
		&v_lite38, &v_lite39, &dragon_egg_light_group, &outlane_kicker_light_group, &tabc21wave, &tabc27wave,
		&tabc35wave, &tabc39wave, &tabc40wave, &tabc41wave, &tabc42wave, &info_text_box, &a_kick1, &a_kout3,
		&a_blocf, &a_bloce, &a_center_post,
	};

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

	// ---- not translated yet ----
	// TODO(23:c5d6): mission event dispatcher (calls 23:1151, 23:1187, 23:c716)
	void MissionEvent(MessageCode code, TPinballComponent* cmp)
	{
		(void)code;
		(void)cmp;
	}

	// TODO(23:44c9 / 23:436f / 23:4215): blocker rules
	void BlocfRule(MessageCode, TPinballComponent*)
	{
	}

	void BloceRule(MessageCode, TPinballComponent*)
	{
	}

	void CenterPostRule(MessageCode, TPinballComponent*)
	{
	}

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
	for (auto tag : AllTags)
		if (!control::make_component_link(*tag))
			ftlog::Line(std::string("FT1 Dragon's Keep rules: component missing on the table: ") + tag->Name);
	Active = true;
	ftlog::Line("FT1 Dragon's Keep: full translation active (work in progress), " + std::to_string(Translated) +
	            " rule functions");
	return Active;
}

void ft1_dragon::Clear()
{
	Active = false;
	Translated = 0;
	g_5fdc = nullptr;
	g_5fe4 = 0;
	Multiplier = 1;
	SuperJackpot = 20000;
	SuperJackpotFlag = false;
}

bool ft1_dragon::Handle(MessageCode code, TPinballComponent* cmp)
{
	if (!Active || !cmp || !cmp->Control)
		return false;
	ControlHandler(code, cmp);
	return true;
}

int ft1_dragon::TranslatedCount()
{
	return Translated;
}
