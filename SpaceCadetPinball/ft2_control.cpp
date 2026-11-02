#include "pch.h"
#include "ft2_control.h"
#include "ft2_scores.h"
#include "ft2_sounds.h"

#include "pb.h"
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

	void drain_collision()
	{
		auto t = table();
		if (t->MultiballCount > 0 || t->TiltLockFlag)
			return; // other balls still in play
		if (t->ExtraBalls)
		{
			t->ExtraBalls--;
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
		return;
	}

	if (code != MessageCode::ControlCollision)
		return;

	if (!t->TiltLockFlag)
	{
		play_component_sound(caller);
		auto points = points_for(caller);
		if (points)
			t->AddScore(points);
	}

	// Captured balls: send them back out after a short pause
	if (dynamic_cast<TKickout*>(caller))
		caller->Message(MessageCode::TKickoutRestartTimer, 1.0f);
	else if (dynamic_cast<TSink*>(caller))
		caller->Message(MessageCode::TSinkResetTimer, 1.0f);
}
