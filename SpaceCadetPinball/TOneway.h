#pragma once
#include "TCollisionComponent.h"

class TLine;

class TOneway : public TCollisionComponent
{
public:
	TOneway(TPinballTable* table, int groupIndex);
	~TOneway() override = default;
	void Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance,
	               TEdgeSegment* edge) override;

	TLine* Line;
	vector2 Pt0{}, Pt1{};
};
