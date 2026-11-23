#include "pch.h"
#include "TTimer.h"

#include "control.h"
#include "timer.h"

TTimer::TTimer(TPinballTable* table, int groupIndex) : TPinballComponent(table, groupIndex, true)
{
	Timer = 0;
}

int TTimer::Message(MessageCode code, float value)
{
	switch (code)
	{
	case MessageCode::Ft1BlockerTimer: // Full Tilt 1 code 69: start for `value` seconds, stop when negative
		if (Timer)
			timer::kill(Timer);
		Timer = value >= 0 ? timer::set(value, this, TimerExpired) : 0;
		break;
	case MessageCode::Ft1TimerTimeLeft: // Full Tilt 1 code 72
		return timer::time_left(Timer);
	case MessageCode::TTimerResetTimer:
		if (Timer)
			timer::kill(Timer);
		Timer = timer::set(value, this, TimerExpired);
		break;
	case MessageCode::SetTiltLock:
	case MessageCode::GameOver:
	case MessageCode::Reset:
		if (Timer)
		{
			timer::kill(Timer);
			Timer = 0;
		}
		break;
	default: break;
	}
	return 0;
}


void TTimer::TimerExpired(int timerId, void* caller)
{
	auto timer = static_cast<TTimer*>(caller);
	timer->Timer = 0;
	control::handler(MessageCode::ControlTimerExpired, timer);
}
