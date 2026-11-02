#include "pch.h"
#include "ft2_parts.h"

#include "loader.h"
#include "TBall.h"
#include "TLine.h"
#include "TPinballTable.h"

TFT2RampExit::TFT2RampExit(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, false)
{
	auto arr = loader::query_float_attribute(groupIndex, 0, 1301);
	if (!arr)
		return;
	TargetGroup = 1 << static_cast<int>(floor(arr[0]));
	BallOffset = arr[7];
	vector2 a{arr[3], arr[4]}, b{arr[5], arr[6]};
	// 1301 = target layer, source layer, point count, two points, ball offset (as Space Cadet's ramp exits).
	// The line only exists on the source layer, from either side. Putting it on every layer also caught balls
	// rolling on the main playfield and moved them to a ramp layer with no floor, so they fell off the table.
	auto source = static_cast<int>(floor(arr[1]));
	int layers = 1 << source;
	if (source == static_cast<int>(floor(arr[0])) || source < 0 || source > 15)
		layers = 0xFFFE & ~TargetGroup;
	for (auto pts : {std::make_pair(a, b), std::make_pair(b, a)})
	{
		auto line = new TLine(this, &ActiveFlag, layers, pts.first, pts.second);
		line->place_in_grid(&AABB);
		EdgeList.push_back(line);
	}
}

void TFT2RampExit::Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance, TEdgeSegment* edge)
{
	// Pass through and continue on the target layer (as Space Cadet's ramps do with their exit lines)
	ball->not_again(edge);
	ball->Position.X = nextPosition->X;
	ball->Position.Y = nextPosition->Y;
	ball->RayMaxDistance -= distance;
	ball->CollisionFlag = 0;
	ball->CollisionMask = TargetGroup;
	ball->Position.Z = ball->Radius + BallOffset;
}

TFT2Diverter::TFT2Diverter(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, false)
{
	visualStruct visual{};
	loader::query_visual(groupIndex, 0, &visual);
	// First position from attribute 604 (count, points..., offset)
	auto arr = loader::query_float_attribute(groupIndex, 0, 604);
	if (!arr)
		return;
	auto count = static_cast<int>(floor(arr[0]));
	for (int i = 0; i + 1 < count; i++)
	{
		vector2 a{arr[1 + i * 2], arr[2 + i * 2]}, b{arr[3 + i * 2], arr[4 + i * 2]};
		for (auto pts : {std::make_pair(a, b), std::make_pair(b, a)})
		{
			auto line = new TLine(this, &ActiveFlag, visual.CollisionGroup, pts.first, pts.second);
			line->Offset(PinballTable->CollisionCompOffset);
			line->place_in_grid(&AABB);
			EdgeList.push_back(line);
		}
	}
}
