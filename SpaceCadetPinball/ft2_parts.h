#pragma once
#include "TCollisionComponent.h"

class TLine;

// Full Tilt 1 parts the Space Cadet decomp never had (Skulduggery, Dragon's Keep):
//  type 1035: ramp exit line - a ball crossing it drops from a ramp layer to another layer (attribute 1301:
//             target layer, enabled, 2 points, ball height offset)
//  type 1043: diverter - a wall the table rules move between positions (attribute 604: one or more lines);
//             without rules it stays in its first position
// (Accelerators, type 1036, are walls with a kick and use TWall.)
class TFT2RampExit : public TCollisionComponent
{
public:
	TFT2RampExit(TPinballTable* table, int groupIndex);
	void Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance, TEdgeSegment* edge) override;

private:
	int TargetGroup = 1;
	float BallOffset = 0;
	// Full Tilt 1 (16:02fa / 16:0650): two lines, one per layer. A ball on layer 1301[0] crossing the first moves to
	// layer 1301[1] (the rules hear 86), one on 1301[1] crossing the second moves to 1301[0] (85)
	bool Ft1 = false;
	int Layer0 = 1, Layer1 = 1;
	TLine* Line0 = nullptr;
	void push_out_of_walls(TBall* ball);
};

class TFT2Diverter : public TCollisionComponent
{
public:
	TFT2Diverter(TPinballTable* table, int groupIndex);
};
