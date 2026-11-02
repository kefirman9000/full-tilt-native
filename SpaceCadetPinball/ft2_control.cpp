#include "pch.h"
#include "ft2_control.h"
#include "ft2_scores.h"
#include "ft2_sounds.h"

#include "pb.h"
#include "loader.h"
#include "options.h"
#include "timer.h"
#include "TLightGroup.h"
#include "translations.h"
#include "TBumper.h"
#include "TDrain.h"
#include "TFlagSpinner.h"
#include "TGate.h"
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
	} rs;

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
		set_pockets(false);
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
		if (enhanced() && rs.BonusHits > 0 && !table()->TiltLockFlag)
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
	}

	void drain_collision()
	{
		auto t = table();
		if (t->MultiballCount > 0 || t->TiltLockFlag)
			return; // other balls still in play
		build_sets();
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
		}
	}
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

	if (code == MessageCode::PlungerInputReleased && dynamic_cast<TPlunger*>(caller))
	{
		play_component_sound(caller);
		build_sets();
		rs.LaunchTick = pb::time_ticks; // ball save counts from the launch
		return;
	}

	if (code != MessageCode::ControlCollision)
		return;

	if (!t->TiltLockFlag)
	{
		play_component_sound(caller);
		auto points = points_for(caller);
		if (points)
		{
			add_points(points);
			rs.BonusHits++;
		}
		if (enhanced())
			set_hit(caller);
	}

	// Captured balls: send them back out after a short pause
	if (dynamic_cast<TKickout*>(caller))
		caller->Message(MessageCode::TKickoutRestartTimer, 1.0f);
	else if (dynamic_cast<TSink*>(caller))
		caller->Message(MessageCode::TSinkResetTimer, 1.0f);
}

void ft2_control::table_handler(MessageCode code)
{
	if (!table())
		return;
	build_sets();
	switch (code)
	{
	case MessageCode::Reset: // new game
		stop_attract();
		if (rs.PocketTimer)
		{
			timer::kill(rs.PocketTimer);
			rs.PocketTimer = 0;
		}
		set_pockets(false);
		for (auto& set : rs.Sets)
			set.Hit.clear();
		for (auto& given : rs.ExtraBallGiven)
			given = false;
		rs.BonusHits = 0;
		rs.Multiplier = 1;
		rs.BallSaveUsed = false;
		rs.LaunchTick = -1000000;
		break;
	case MessageCode::GameOver:
		if (enhanced() && !rs.AttractTimer)
			rs.AttractTimer = timer::set(3.5f, nullptr, attract_tick); // lights play while waiting for a new game
		break;
	case MessageCode::SwitchToNextPlayer:
		for (auto& set : rs.Sets)
			set.Hit.clear();
		break;
	default:
		break;
	}
}
