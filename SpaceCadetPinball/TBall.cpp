#include "pch.h"
#include "GroupData.h"
#include "options.h"
#include "TBall.h"


#include "fullscrn.h"
#include "loader.h"
#include "marble_balls.h"
#include "maths.h"
#include "pb.h"
#include "proj.h"
#include "render.h"
#include "TPinballTable.h"
#include "TTableLayer.h"

TBall::TBall(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, false),
                                                     TEdgeSegment(this, &ActiveFlag, 0)
{
	visualStruct visual{};
	char ballGroupName[10]{"ball"};

	RayMaxDistance = 0.0;
	ActiveFlag = 1;
	CollisionComp = nullptr;
	EdgeCollisionCount = 0;
	TimeDelta = 0.0;
	CollisionFlag = 0;
	Speed = 0.0;
	Direction.Y = 0.0;
	Direction.X = 0.0;
	ListBitmap = new std::vector<SpriteData>();

	if (groupIndex == -1)
	{
		HasGroupFlag = false;
		Position = {0, 0, 0};
		CollisionMask = 1;
	}
	else
	{
		HasGroupFlag = true;
		loader::query_visual(groupIndex, 0, &visual);
		CollisionMask = visual.CollisionGroup;
		auto floatArr = loader::query_float_attribute(groupIndex, 0, 408);
		Position = {floatArr[0], floatArr[1], floatArr[3]};
	}

	/*Full tilt: ball is ballN, where N[0,2] resolution. Some Full Tilt 1 tables also have a plain "ball" group, a
	  smiley face; the real ball (ballN) comes first, the smiley is a ball style.*/
	ballGroupName[4] = '0' + fullscrn::GetResolution();
	groupIndex = loader::query_handle(ballGroupName);
	if (groupIndex < 0)
	{
		ballGroupName[4] = 0;
		groupIndex = loader::query_handle(ballGroupName);
	}

	Radius = *loader::query_float_attribute(groupIndex, 0, 500);
	GroupIndex = groupIndex;
	SetStyle(options::Options.BallStyle.V);
	RenderSprite = new render_sprite(VisualTypes::Ball, nullptr, nullptr, 0, 0, nullptr);
	PinballTable->CollisionCompOffset = Radius;
	Position.Z = Radius;
}

static int BallGroup(const std::string& style);

static bool SmileyAltAvailable()
{
	auto group = BallGroup("");
	if (group < 0)
		return false;
	auto count = loader::query_visual_states(group);
	for (int index = 0; index < count && index < 50; index++)
	{
		auto stateId = loader::state_id(group, index);
		if (stateId < 0 || !loader::loader_table->GetAltBitmap(stateId))
			return false;
	}
	return count > 0;
}

static int BallGroup(const std::string& style)
{
	// "smiley" = the plain "ball" group some Full Tilt 1 tables have next to the real ballN
	if (style == "smiley")
	{
		auto plain = loader::query_handle("ball");
		auto real = loader::query_handle(("ball" + std::to_string(fullscrn::GetResolution())).c_str());
		return plain >= 0 && real >= 0 ? plain : -1;
	}
	// "smiley_alt" (shown as Smiley): Dragon's Keep and Skulduggery keep the smiley as a second picture in each ball frame
	if (style == "smiley_alt")
		return SmileyAltAvailable() ? BallGroup("") : -1;
	std::string name = (style.empty() ? "" : style + "_") + "ball";
	auto group = loader::query_handle((name + std::to_string(fullscrn::GetResolution())).c_str());
	if (group < 0)
		group = loader::query_handle(name.c_str());
	return group;
}

void TBall::SetStyle(const std::string& style)
{
	// Picture only: the ball's size and physics stay those of the standard ball
	if (marbles::IsMarbleStyle(style))
	{
		// A Marble Drop marble scaled to each of the standard ball's depth frames (sizes and depths stay the table's)
		auto standard = BallGroup("");
		if (standard >= 0)
		{
			visualStruct visual{};
			std::vector<gdrv_bitmap8*> frames, marble;
			std::vector<float> depths;
			std::vector<zmap_header_type*> zmaps;
			auto visualCount = loader::query_visual_states(standard);
			for (auto index = 0; index < visualCount && index < 50; ++index)
			{
				loader::query_visual(standard, index, &visual);
				frames.push_back(visual.Bitmap.Bmp);
				zmaps.push_back(visual.Bitmap.ZMap);
				auto visVec = reinterpret_cast<vector3*>(loader::query_float_attribute(standard, index, 501));
				depths.push_back(visVec ? proj::z_distance(*visVec) : 0.0f);
			}
			if (marbles::Build(style, frames, marble))
			{
				ListBitmap->clear();
				for (size_t index = 0; index < marble.size(); ++index)
				{
					ListBitmap->push_back(SpriteData{marble[index], zmaps[index]});
					VisualZArray[index] = depths[index];
				}
				return;
			}
		}
		// No Marble Drop data (or it could not be read): fall back to the standard ball
	}
	auto groupIndex = BallGroup(marbles::IsMarbleStyle(style) ? "" : style);
	if (groupIndex < 0)
		groupIndex = BallGroup("");
	if (groupIndex < 0)
		return;
	visualStruct visual{};
	ListBitmap->clear();
	auto alt = style == "smiley_alt" && SmileyAltAvailable();
	auto visualCount = loader::query_visual_states(groupIndex);
	for (auto index = 0; index < visualCount && index < 50; ++index)
	{
		loader::query_visual(groupIndex, index, &visual);
		if (alt)
			visual.Bitmap.Bmp = loader::loader_table->GetAltBitmap(loader::state_id(groupIndex, index));
		ListBitmap->push_back(visual.Bitmap);
		auto visVec = reinterpret_cast<vector3*>(loader::query_float_attribute(groupIndex, index, 501));
		VisualZArray[index] = visVec ? proj::z_distance(*visVec) : 0.0f;
	}
}

std::vector<std::string> TBall::AvailableStyles()
{
	// Ball sets in the table data are named "<style>_ball<resolution>" next to the standard "ball<resolution>"
	std::vector<std::string> styles;
	auto suffix = "_ball" + std::to_string(fullscrn::GetResolution());
	for (auto group : loader::loader_table->Groups)
	{
		if (!group)
			continue;
		auto& name = group->GroupName;
		if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
			styles.push_back(name.substr(0, name.size() - suffix.size()));
	}
	if (BallGroup("smiley") >= 0)
		styles.push_back("smiley");
	else if (SmileyAltAvailable())
		styles.push_back("smiley_alt");
	// Marble Drop's marbles, when its files are in tables/_MarbleDrop (see marble_balls.h)
	for (auto& marble : marbles::Styles())
		styles.push_back(marble);
	return styles;
}

void TBall::Repaint()
{
	if (CollisionFlag)
	{
		Position.Z =
			CollisionOffset.X * Position.X +
			CollisionOffset.Y * Position.Y +
			Radius + CollisionOffset.Z;
	}

	auto pos2D = proj::xform_to_2d(Position);
	auto zDepth = proj::z_distance(Position);

	auto index = 0u;
	for (; index < ListBitmap->size() - 1; ++index)
	{
		if (VisualZArray[index] <= zDepth) break;
	}

	if (pb::FT2Table && getenv("FT2_DEBUG"))
	{
		static int n;
		if (++n % 60 == 0)
			fprintf(stderr, "FT2DBG repaint 2d %d %d z %.2f idx %u/%zu zarr0 %.2f last %.2f\n", pos2D.X, pos2D.Y, zDepth, index,
			        ListBitmap->size(), VisualZArray[0], VisualZArray[ListBitmap->size() - 1]);
	}
	SpriteSetBall(index, pos2D, zDepth);
}

void TBall::not_again(TEdgeSegment* edge)
{
	if (EdgeCollisionCount < 16)
	{
		Collisions[EdgeCollisionCount] = edge;
		++EdgeCollisionCount;
	}
	else
	{
		for (int i = 0; i < 8; i++)
			Collisions[i] = Collisions[i + 8];
		Collisions[8] = edge;
		EdgeCollisionCount = 9;
	}
	EdgeCollisionResetFlag = true;
}

bool TBall::already_hit(const TEdgeSegment& edge) const
{
	for (int i = 0; i < EdgeCollisionCount; i++)
	{
		if (Collisions[i] == &edge)
			return true;
	}

	return false;
}

int TBall::Message(MessageCode code, float value)
{
	if (code == MessageCode::Reset)
	{
		SpriteSetBall(-1, {0, 0}, 0.0f);
		Position.X = 0.0;
		CollisionComp = nullptr;
		Position.Y = 0.0;
		ActiveFlag = 0;
		CollisionFlag = 0;
		CollisionMask = 1;
		Direction.Y = 0.0;
		Position.Z = Radius;
		Direction.X = 0.0;
		Speed = 0.0;
		RayMaxDistance = 0.0;
	}
	return 0;
}

void TBall::throw_ball(vector3* direction, float angleMult, float speedMult1, float speedMult2)
{
	CollisionComp = nullptr;
	Direction = *direction;
	float rnd = RandFloat();
	float angle = (1.0f - (rnd + rnd)) * angleMult;
	maths::RotateVector(Direction, angle);
	rnd = RandFloat();
	Speed = (1.0f - (rnd + rnd)) * (speedMult1 * speedMult2) + speedMult1;
}

void TBall::EdgeCollision(TBall* ball, float distance)
{
	ball->CollisionDisabledFlag = true;
	ball->Position.X += ball->Direction.X * distance;
	ball->Position.Y += ball->Direction.Y * distance;
	ball->Direction.X *= ball->Speed;
	ball->Direction.Y *= ball->Speed;
	Direction.X *= Speed;
	Direction.Y *= Speed;

	// AB - vector from ball to this, BA - from this to ball; collision direction
	vector2 AB{ball->Position.X - Position.X, ball->Position.Y - Position.Y};
	maths::normalize_2d(AB);
	vector2 BA{-AB.X, -AB.Y};

	// Projection = difference between ball directions and collision direction
	auto projAB = -maths::DotProduct(ball->Direction, AB);
	auto projBA = -maths::DotProduct(Direction, BA);
	vector2 delta{AB.X * projAB - BA.X * projBA, AB.Y * projAB - BA.Y * projBA};

	ball->Direction.X += delta.X;
	ball->Direction.Y += delta.Y;
	ball->Speed = maths::normalize_2d(ball->Direction);
	Direction.X -= delta.X;
	Direction.Y -= delta.Y;
	Speed = maths::normalize_2d(Direction);
}

float TBall::FindCollisionDistance(const ray_type& ray)
{
	// Original inherits TCircle and aliases position.
	const circle_type ballCircle{{Position.X, Position.Y}, Radius * Radius * 4.0f};
	return maths::ray_intersect_circle(ray, ballCircle);
}

vector2 TBall::get_coordinates()
{
	return TTableLayer::edge_manager->NormalizeBox(Position);
}

void TBall::Disable()
{
	ActiveFlag = false;
	CollisionDisabledFlag = true;
	SpriteSet(-1);
}
