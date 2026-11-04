#pragma once
#include "TCollisionComponent.h"

class TPlunger :
	public TCollisionComponent
{
public:
	TPlunger(TPinballTable* table, int groupIndex);
	~TPlunger() override = default;
	void Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance,
	               TEdgeSegment* edge) override;
	int Message(MessageCode code, float value) override;

	static void BallFeedTimer(int timerId, void* caller);
	static void PullbackTimer(int timerId, void* caller);
	static void ReleasedTimer(int timerId, void* caller);

	int PullbackTimer_;
	int BallFeedTimer_;
	float MaxPullback;
	float PullbackIncrement;
	float PullbackDelay;
	int SoundIndexP1;
	int SoundIndexP2;
	bool PullbackStartedFlag{};
	int SomeCounter{};

	// FT2 tables: launch the ball directly on release (see TPlunger.cpp)
	void FT2Launch();
	bool DirectLaunch() const;
	vector2 FT2Center{};
	float FT2Reach{};
	// Tables with a skill shot (Captain Hero): the low end of the pull is spread over the soft shots, from one that
	// falls short to one that only just reaches the top of the lane (set by the table rules; 0 = off)
	float SkillShotLow{};
	std::vector<vector2> FT2Shape;
};
