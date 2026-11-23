#include "pch.h"
#include "ft2_parts.h"

#include "loader.h"
#include "control.h"
#include "pb.h"
#include "TBall.h"
#include "TLine.h"
#include "TPinballTable.h"

TFT2RampExit::TFT2RampExit(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, false)
{
	auto arr = loader::query_float_attribute(groupIndex, 0, 1301);
	if (!arr)
		return;
	if (!pb::FT2FullScreen)
	{
		Ft1 = true;
		Layer0 = 1 << static_cast<int>(floor(arr[0]));
		Layer1 = 1 << static_cast<int>(floor(arr[1]));
		BallOffset = arr[7];
		vector2 a{arr[3], arr[4]}, b{arr[5], arr[6]};
		Line0 = new TLine(this, &ActiveFlag, Layer0, a, b);
		Line0->place_in_grid(&AABB);
		EdgeList.push_back(Line0);
		auto line1 = new TLine(this, &ActiveFlag, Layer1, b, a);
		line1->place_in_grid(&AABB);
		EdgeList.push_back(line1);
		return;
	}
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

// A ball that changes layer at an exit line may already be just behind a wall of its new layer that starts before
// the line (Skulduggery's habitrail behind a_blocf: its funnel's side walls lie across the exit line). Walls only
// stop balls from their front, so a fast ball went on through and off the table. Put it back in front, bouncing.
void TFT2RampExit::push_out_of_walls(TBall* ball)
{
	auto table = PinballTable;
	auto& p = ball->Position;
	for (auto part : table->ComponentList)
	{
		auto comp = dynamic_cast<TCollisionComponent*>(part);
		if (!comp || comp == this)
			continue;
		for (auto edge : comp->EdgeList)
		{
			auto line = dynamic_cast<TLine*>(edge);
			if (!line || !(line->CollisionGroup & ball->CollisionMask) || !line->ActiveFlagPtr || !*line->ActiveFlagPtr)
				continue;
			auto& l = line->Line;
			vector2 n{l.Direction.Y, -l.Direction.X}; // the side rays hit it from (maths::ray_intersect_line)
			auto s = (p.X - l.Origin.X) * n.X + (p.Y - l.Origin.Y) * n.Y;
			if (s >= 0.0f || s < -ball->Radius * 2.0f)
				continue;
			auto along = l.Direction.X != 0.0f ? p.X - s * n.X : p.Y - s * n.Y; // the foot on the line
			if (along < l.MinCoord || along > l.MaxCoord)
				continue;
			p.X += n.X * (0.01f - s);
			p.Y += n.Y * (0.01f - s);
			auto into = ball->Direction.X * n.X + ball->Direction.Y * n.Y;
			if (into < 0.0f)
			{
				ball->Direction.X -= 2.0f * into * n.X;
				ball->Direction.Y -= 2.0f * into * n.Y;
			}
		}
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
	if (Ft1)
	{
		bool first = edge == Line0;
		ball->CollisionMask = first ? Layer1 : Layer0; // the height stays as it is (16:0650 sets only the layer)
		// End this step here: the rest of it was found against the old layer's walls
		ball->RayMaxDistance = 0.0f;
		push_out_of_walls(ball);
		control::handler(first ? MessageCode::Ft1RampExit3 : MessageCode::Ft1RampExit2, this);
		return;
	}
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
