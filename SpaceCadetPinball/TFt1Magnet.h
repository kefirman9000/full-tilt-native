#pragma once
#include "TKickout.h"

// Full Tilt 1 magnet (type 1042, Skulduggery's s_magnet1-6; engine 15:5e49): a kickout's pull (15:5b26) without the
// kickout. When the ball touches it (15:5eb1) the ball rolls on, the pull stops for 1 s and the rules hear
// ControlCollision (FT1 74); when the second is up (15:5fea) the pull is back and the rules hear ControlTimerExpired
// (FT1 70). Ft1Activate / Ft1Deactivate (FT1 75 / 76, 15:602b) switch it on and off; tilt, game over and reset
// switch it off.
class TFt1Magnet :
	public TKickout
{
public:
	TFt1Magnet(TPinballTable* table, int groupIndex);
	int Message(MessageCode code, float value) override;
	void Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance,
	               TEdgeSegment* edge) override;
	static void HoldExpired(int timerId, void* caller);
	float HoldTime = 1.0f; // +0x74
};
