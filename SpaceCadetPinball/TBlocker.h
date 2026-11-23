#pragma once
#include "TCollisionComponent.h"

class TBlocker :
	public TCollisionComponent
{
public:
	TBlocker(TPinballTable* table, int groupIndex);
	int Message(MessageCode code, float value) override;

	static void TimerExpired(int timerId, void* caller);

	// Full Tilt 1 (Dragon's Keep, Skulduggery): the multi-position blocker of 15:3a83 / 15:3d94
	bool Ft1 = false;
	int FrameCount = 0;
	int Frame = -1;         // +0x70
	int InitialFrame = -1;  // +0x72, attribute 409
	int TargetFrame = -1;   // +0x7c
	float StepDelay = 0.1f; // +0x7e, attribute 903
	int MoveTimer = 0;      // +0x82
	int RuleTimer = 0;      // +0x78
	char FrameActive[8]{};  // +0x68: the walls of each frame
	void Ft1SetPosition(int frame);
	void Ft1MoveTo(int frame, float delay);
	static void Ft1Step(int timerId, void* caller);
	static void Ft1RuleTimer(int timerId, void* caller);

	int InitialDuration;
	int ExtendedDuration;
	int Timer;
	int SoundIndex4;
	int SoundIndex3;
};
