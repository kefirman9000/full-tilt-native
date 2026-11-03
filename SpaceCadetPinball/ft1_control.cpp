// Full Tilt 1 extra tables: rules recovered from the original programs (see ft1_control.h).
//
// All Full Tilt 1 tables share one engine; only code segment 23 (the rules) differs. Each function here is a port of
// one rule function of that segment; the comment above it names the original address (DRAGON.EXE 23:xxxx) so it can
// be checked against the listing ft2/tools/ft1draft.py produces from the player's own exe.
//
// Message codes: Full Tilt 1 numbers them differently from the Windows XP build this engine comes from; the ports use
// this engine's MessageCode names (map learned from CADET.EXE + control.cpp, ft2/tools/ft1_names.txt). Codes Full Tilt
// 1 has and the XP build does not get private values here (FT1Msg).

#include "pch.h"
#include "ft1_control.h"

#include "control.h"
#include "ft1_strings.h"
#include "ft_log.h"
#include "pb.h"
#include "TPinballComponent.h"
#include "TPinballTable.h"

namespace
{
	// Full Tilt 1 messages without an equivalent in this engine, used between recovered rules only
	namespace FT1Msg
	{
		// FT1 code 77: sent to the previously hit component when the ball scores on a different one
		const MessageCode LastHitMovedOn = static_cast<MessageCode>(2077);
	}

	bool Active = false;
	int Ported = 0;

	// FT1 global (DRAGON.EXE data 5fdc): the component that scored last
	TPinballComponent* LastHit = nullptr;

	// DRAGON.EXE 23:06cf - score a hit: tell the previous scorer the ball moved on, then add the component's score
	void ScoreHit(TPinballComponent* cmp, int index)
	{
		if (cmp != LastHit)
		{
			if (LastHit)
				control::handler(FT1Msg::LastHitMovedOn, LastHit);
			LastHit = cmp;
		}
		control::TableG->AddScore(cmp->get_scoring(index));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Dragon's Keep (DRAGON.DAT / DRAGON.EXE)
	// ---------------------------------------------------------------------------------------------------------------
	namespace dragon
	{
		component_tag<> lite19{"v_lite19"}, lite20{"v_lite20"}, lite120{"v_lite120"}, lite121{"v_lite121"};
		component_tag<> rebo1{"a_rebo1"}, rebo2{"a_rebo2"}, spot30{"a_spot30"}, spot31{"a_spot31"};

		// DRAGON.EXE 23:2ced / 23:2d4c - rebounders: their light blinks, then the hit scores
		void Rebounder1(MessageCode code, TPinballComponent* caller)
		{
			if (code == MessageCode::ControlCollision)
			{
				lite120.Component->Message(MessageCode::TLightTurnOnTimed, 0.1f);
				ScoreHit(caller, 0);
			}
		}

		void Rebounder2(MessageCode code, TPinballComponent* caller)
		{
			if (code == MessageCode::ControlCollision)
			{
				lite121.Component->Message(MessageCode::TLightTurnOnTimed, 0.1f);
				ScoreHit(caller, 0);
			}
		}

		// DRAGON.EXE 23:4157 / 23:41b6 - spot targets 30 and 31: flash their light for 2 s, score
		void Spot30(MessageCode code, TPinballComponent* caller)
		{
			if (code == MessageCode::ControlCollision)
			{
				lite20.Component->Message(MessageCode::TLightFlasherStartTimed, 2.0f);
				ScoreHit(caller, 0);
			}
		}

		void Spot31(MessageCode code, TPinballComponent* caller)
		{
			if (code == MessageCode::ControlCollision)
			{
				lite19.Component->Message(MessageCode::TLightFlasherStartTimed, 2.0f);
				ScoreHit(caller, 0);
			}
		}

		// scores as recovered from the rule table (ft2/dragon_rules.txt)
		const int score1000[] = {1000};
		const int score2500[] = {2500};

		component_info rules[] =
		{
			{rebo1, {Rebounder1, 1, score1000}},
			{rebo2, {Rebounder2, 1, score1000}},
			{spot30, {Spot30, 1, score2500}},
			{spot31, {Spot31, 1, score2500}},
		};

		// components the rules use without having a rule of their own
		component_tag_base* used[] = {&lite19, &lite20, &lite120, &lite121};
	}

	bool IsTable(const char* datName)
	{
		std::string t = pb::TableFile;
		auto slash = t.find_last_of("/\\");
		if (slash != std::string::npos)
			t = t.substr(slash + 1);
		for (auto& c : t)
			c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
		return t == datName;
	}

	template <size_t N, size_t U>
	bool LinkRules(component_info (&rules)[N], component_tag_base* (&used)[U])
	{
		// a rule is only enabled if every component it uses exists on the table
		for (auto tag : used)
			if (!control::make_component_link(*tag))
			{
				ftlog::Line(std::string("FT1 rules: component missing on the table: ") + tag->Name);
				return false;
			}
		for (auto& rule : rules)
		{
			auto comp = control::make_component_link(rule.Tag);
			if (comp)
			{
				comp->Control = &rule.Control;
				Ported++;
			}
		}
		return true;
	}
}

bool ft1_control::Link(TPinballTable* table)
{
	Clear();
	if (IsTable("DRAGON.DAT"))
	{
		auto exe = pb::make_path_name("DRAGON.EXE");
		if (!ft1_strings::Load(exe))
			ftlog::Line("FT1 rules: DRAGON.EXE not found next to DRAGON.DAT; table messages will be missing");
		Active = LinkRules(dragon::rules, dragon::used);
	}
	if (Active)
		ftlog::Line("FT1 rules: " + std::to_string(Ported) + " components use recovered rules");
	return Active;
}

void ft1_control::Clear()
{
	Active = false;
	Ported = 0;
	LastHit = nullptr;
}

bool ft1_control::Handle(MessageCode code, TPinballComponent* cmp)
{
	if (!Active || !cmp || !cmp->Control)
		return false;
	if (pb::DevDebug)
		ftlog::Line("FT1 rule: " + std::string(cmp->GroupName ? cmp->GroupName : "?") + " message " +
		            std::to_string(static_cast<int>(code)));
	cmp->Control->ControlFunc(code, cmp);
	return true;
}

int ft1_control::PortedCount()
{
	return Ported;
}
