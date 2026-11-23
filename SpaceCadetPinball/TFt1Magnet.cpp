#include "pch.h"
#include "TFt1Magnet.h"

#include "control.h"
#include "TBall.h"
#include "timer.h"

TFt1Magnet::TFt1Magnet(TPinballTable* table, int groupIndex) : TKickout(table, groupIndex, true)
{
	BallCaputeredFlag = 0; // +0x68: the pull is on
}

int TFt1Magnet::Message(MessageCode code, float value)
{
	switch (code)
	{
	case MessageCode::Ft1Activate: // FT1 75
		ActiveFlag = 1;
		break;
	case MessageCode::Ft1Deactivate: // FT1 76
	case MessageCode::SetTiltLock:
	case MessageCode::GameOver:
	case MessageCode::Reset:
		if (Timer)
			timer::kill(Timer);
		Timer = 0;
		ActiveFlag = 0;
		BallCaputeredFlag = 0;
		break;
	default:
		break;
	}
	return 0;
}

void TFt1Magnet::Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance,
                           TEdgeSegment* edge)
{
	ball->Position.X = nextPosition->X;
	ball->Position.Y = nextPosition->Y;
	ball->RayMaxDistance -= distance;
	ball->not_again(edge);
	BallCaputeredFlag = 1;
	if (Timer)
		timer::kill(Timer);
	Timer = timer::set(HoldTime, this, HoldExpired);
	control::handler(MessageCode::ControlCollision, this);
}

void TFt1Magnet::HoldExpired(int timerId, void* caller)
{
	auto magnet = static_cast<TFt1Magnet*>(caller);
	magnet->BallCaputeredFlag = 0;
	magnet->Timer = 0;
	control::handler(MessageCode::ControlTimerExpired, magnet);
}
