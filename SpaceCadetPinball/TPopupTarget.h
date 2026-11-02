#pragma once
#include "TCollisionComponent.h"

class TPopupTarget :
	public TCollisionComponent
{
public:
	TPopupTarget(TPinballTable* table, int groupIndex);
	int Message(MessageCode code, float value) override;
	void Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance,
		TEdgeSegment* edge) override;	

	static void TimerExpired(int timerId, void* caller);

	int Timer;
	float TimerTime;
	int PlayerMessagefieldBackup[4]{};
	// FT2: targets flagged with attribute 1800 (Alien's Bigfoot creatures) only appear during a mode; they start hidden
	bool HiddenUntilMode = false;
};
