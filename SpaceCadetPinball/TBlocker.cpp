#include "pch.h"
#include "TBlocker.h"


#include "control.h"
#include "loader.h"
#include "render.h"
#include "timer.h"
#include "TPinballTable.h"
#include "TEdgeSegment.h"
#include "pb.h"

TBlocker::TBlocker(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, true)
{
	visualStruct visual{};

	loader::query_visual(groupIndex, 0, &visual);
	SoundIndex4 = visual.SoundIndex4;
	SoundIndex3 = visual.SoundIndex3;
	InitialDuration = 55;
	ExtendedDuration = 5;
	Threshold = 1000000000.0f;
	Timer = 0;
	MessageField = 0;
	ActiveFlag = 0;
	SpriteSet(-1);

	// Full Tilt 1 tables (Full Tilt engine, Space Cadet layout): every state of the group is a position with its own
	// wall (15:3a83: up to 8; the walls switch with +0x68[frame])
	Ft1 = pb::FT2Table && !pb::FT2FullScreen && groupIndex > 0;
	if (Ft1)
	{
		FrameCount = std::max(1, std::min(8, static_cast<int>(loader::query_visual_states(groupIndex))));
		for (auto edge : EdgeList)
			edge->ActiveFlagPtr = &FrameActive[0];
		auto offset = table->CollisionCompOffset;
		for (int frame = 1; frame < FrameCount; frame++)
			if (auto walls = loader::query_float_attribute(groupIndex, frame, 600))
			{
				visualStruct frameVisual{};
				loader::query_visual(groupIndex, frame, &frameVisual);
				TEdgeSegment::install_wall(walls, this, &FrameActive[frame], frameVisual.CollisionGroup, offset, 0);
			}
		if (auto delay = loader::query_float_attribute(groupIndex, 0, 903))
			StepDelay = delay[0];
		if (auto durations = loader::query_float_attribute(groupIndex, 0, 407)) // +0x74 / +0x76
		{
			InitialDuration = static_cast<int>(durations[0]);
			ExtendedDuration = static_cast<int>(durations[1]);
		}
		if (auto initial = loader::query_float_attribute(groupIndex, 0, 409))
			InitialFrame = static_cast<int>(std::lround(initial[0]));
		if (pb::DevDebug && getenv("FT1_BLOCKER_DOWN") && GroupName && !strcmp(GroupName, getenv("FT1_BLOCKER_DOWN")))
			InitialFrame = -1; // development: start with this blocker out of the way (for test shots)
		ActiveFlag = 1; // the frames' own flags decide which wall is there
		Ft1SetPosition(InitialFrame);
		if (pb::DevDebug)
			printf("FT2DBG ft1 blocker %s frames %d initial %d delay %.2f edges %d\n", GroupName ? GroupName : "?",
			       FrameCount, InitialFrame, StepDelay, static_cast<int>(EdgeList.size()));
	}
}

void TBlocker::Ft1SetPosition(int frame)
{
	if (MoveTimer)
	{
		timer::kill(MoveTimer);
		MoveTimer = 0;
	}
	if (frame >= FrameCount)
		frame = FrameCount - 1;
	if (Frame >= 0 && Frame < FrameCount)
		FrameActive[Frame] = 0;
	Frame = frame < 0 ? -1 : frame;
	if (Frame >= 0)
		FrameActive[Frame] = 1;
	SpriteSet(Frame);
}

void TBlocker::Ft1MoveTo(int frame, float delay)
{
	TargetFrame = frame;
	if (MoveTimer)
		timer::kill(MoveTimer);
	MoveTimer = timer::set(delay, this, Ft1Step);
}

// 15:3cd3: one frame towards the target every StepDelay seconds; at the target the rules hear of it (FT1 61)
void TBlocker::Ft1Step(int timerId, void* caller)
{
	auto blocker = static_cast<TBlocker*>(caller);
	blocker->MoveTimer = 0;
	if (blocker->TargetFrame == blocker->Frame)
	{
		control::handler(MessageCode::Ft1BlockerArrived, blocker);
		return;
	}
	auto next = blocker->TargetFrame > blocker->Frame ? blocker->Frame + 1 : blocker->Frame - 1;
	blocker->Ft1SetPosition(next);
	blocker->MoveTimer = timer::set(blocker->StepDelay, blocker, Ft1Step);
}

void TBlocker::Ft1RuleTimer(int timerId, void* caller)
{
	auto blocker = static_cast<TBlocker*>(caller);
	blocker->RuleTimer = 0;
	control::handler(MessageCode::ControlTimerExpired, blocker); // FT1 70, as from a timer
}

int TBlocker::Message(MessageCode code, float value)
{
	if (Ft1)
	{
		// 15:3d94
		switch (code)
		{
		case MessageCode::GameOver:
		case MessageCode::Reset:
		case MessageCode::SetTiltLock:
			if (RuleTimer)
				timer::kill(RuleTimer);
			RuleTimer = 0;
			Ft1SetPosition(InitialFrame);
			break;
		case MessageCode::Ft1BlockerHide: // 60
			Ft1SetPosition(-1);
			loader::play_sound(SoundIndex3, this, "TBlocker1");
			break;
		case MessageCode::Ft1BlockerShowLast: // 59
			Ft1SetPosition(FrameCount - 1);
			loader::play_sound(SoundIndex4, this, "TBlocker2");
			break;
		case MessageCode::TBlockerDisable: // 55: down, frame by frame, at once or after `value` seconds
			Ft1MoveTo(-1, value > 0 ? value : StepDelay);
			loader::play_sound(SoundIndex3, this, "TBlocker1");
			break;
		case MessageCode::TBlockerEnable: // 56: up to the last frame
			Ft1MoveTo(FrameCount - 1, value > 0 ? value : StepDelay);
			loader::play_sound(SoundIndex4, this, "TBlocker2");
			break;
		case MessageCode::Ft1BlockerMoveTo: // 58
			Ft1MoveTo(static_cast<int>(std::lround(value)), StepDelay);
			break;
		case MessageCode::Ft1BlockerSetPosition: // 57
			Ft1SetPosition(static_cast<int>(std::lround(value)));
			break;
		case MessageCode::Ft1BlockerTimer: // 69
			if (RuleTimer)
				timer::kill(RuleTimer);
			RuleTimer = timer::set(std::max(value, 0.0f), this, Ft1RuleTimer);
			break;
		default:
			break;
		}
		return 0;
	}
	switch (code)
	{
	case MessageCode::SetTiltLock:
	case MessageCode::PlayerChanged:
	case MessageCode::Reset:
	case MessageCode::TBlockerDisable:
		if (Timer)
		{
			timer::kill(Timer);
			Timer = 0;
		}
		MessageField = 0;
		ActiveFlag = 0;
		SpriteSet(-1);
		if (code == MessageCode::TBlockerDisable)
			loader::play_sound(SoundIndex3, this, "TBlocker1");
		break;
	case MessageCode::TBlockerEnable:
		ActiveFlag = 1;
		loader::play_sound(SoundIndex4, this, "TBlocker2");
		SpriteSet(0);
		if (Timer)
			timer::kill(Timer);
		Timer = 0;
		if (value >= 0)
			Timer = timer::set(value, this, TimerExpired);
		break;
	case MessageCode::TBlockerRestartTimeout:
		if (Timer)
			timer::kill(Timer);
		Timer = timer::set(std::max(value, 0.0f), this, TimerExpired);
		break;
	default:
		break;
	}
	
	return 0;
}

void TBlocker::TimerExpired(int timerId, void* caller)
{
	auto blocker = static_cast<TBlocker*>(caller);
	blocker->Timer = 0;
	control::handler(MessageCode::ControlTimerExpired, blocker);
}
