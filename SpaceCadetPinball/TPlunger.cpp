#include "pch.h"
#include "TPlunger.h"


#include "control.h"
#include "loader.h"
#include "maths.h"
#include "pb.h"
#include "render.h"
#include "TBall.h"
#include "timer.h"
#include "TPinballTable.h"

TPlunger::TPlunger(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, true)
{
	visualStruct visual{};

	loader::query_visual(groupIndex, 0, &visual);
	Boost = 0.0;
	BallFeedTimer_ = 0;
	PullbackTimer_ = 0;
	SoundIndexP1 = visual.SoundIndex4;
	SoundIndexP2 = visual.SoundIndex3;
	HardHitSoundId = visual.Kicker.HardHitSoundId;
	Threshold = 1000000000.0;

    // FT:MaxPullback = 50; 3DPB: MaxPullback = 100, PullbackIncrement is floored.
    if (pb::FullTiltMode)
    {
        MaxPullback = 50;
        // FT2 tables give the plunger strength in attribute 402 (500..1000; their world is ~10x Cadet's)
        if (pb::FT2Table)
        {
            auto strength = loader::query_float_attribute(groupIndex, 0, 402);
            if (strength)
                MaxPullback = strength[0];
        }
        PullbackIncrement = MaxPullback / ((ListBitmap ? ListBitmap->size() : 8) * 8.0f);
        if (pb::DevDebug)
            printf("FT2DBG plunger max pullback %.1f\n", MaxPullback);
    }
    else
    {
        MaxPullback = 100;
        PullbackIncrement = std::floor(MaxPullback / ((ListBitmap ? ListBitmap->size() : 8) * 8.0f));
    }

	Elasticity = 0.5f;
	Smoothness = 0.5f;
	
	PullbackDelay = 0.025f;
	float* floatArr = loader::query_float_attribute(groupIndex, 0, 601);
	table->PlungerPosition = {floatArr[0], floatArr[1]};

	if (DirectLaunch())
	{
		// Plunger shape (attribute 600: count, then x/y pairs): center and size, used by FT2Launch
		auto wall = loader::query_float_attribute(groupIndex, 0, 600);
		if (wall)
		{
			int count = static_cast<int>(wall[0]);
			float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
			for (int i = 0; i < count; i++)
			{
				FT2Shape.push_back({wall[1 + i * 2], wall[2 + i * 2]});
				minX = std::min(minX, wall[1 + i * 2]);
				maxX = std::max(maxX, wall[1 + i * 2]);
				minY = std::min(minY, wall[2 + i * 2]);
				maxY = std::max(maxY, wall[2 + i * 2]);
			}
			FT2Center = {(minX + maxX) / 2, (minY + maxY) / 2};
			FT2Reach = std::max(maxX - minX, maxY - minY);
		}
	}
}

// FT2 tables and picture-less launchers (Skulduggery's cannon) push the ball directly on release, see FT2Launch
bool TPlunger::DirectLaunch() const
{
	return pb::FT2FullScreen || (pb::FT2Table && !ListBitmap);
}

void TPlunger::FT2Launch()
{
	// FT2 plungers are angled shapes (Alien's ray gun fires the ball sideways through a one-way gate) that the resting
	// ball barely touches, so the Cadet-style "hit during the release window" rarely happens. Instead, push any ball
	// resting against the plunger away from the nearest face of the plunger shape.
	if (FT2Shape.size() < 2)
		return;
	for (auto ball : PinballTable->BallList)
	{
		if (!ball->ActiveFlag || ball->CollisionComp)
			continue;
		float bestDist = 1e9f;
		vector2 bestNormal{};
		for (size_t i = 0; i + 1 < FT2Shape.size(); i++)
		{
			auto a = FT2Shape[i], b = FT2Shape[i + 1];
			vector2 ab{b.X - a.X, b.Y - a.Y};
			auto len2 = ab.X * ab.X + ab.Y * ab.Y;
			if (len2 <= 0.0f)
				continue;
			auto t = std::max(0.0f, std::min(1.0f, ((ball->Position.X - a.X) * ab.X + (ball->Position.Y - a.Y) * ab.Y) / len2));
			vector2 closest{a.X + ab.X * t, a.Y + ab.Y * t};
			vector2 away{ball->Position.X - closest.X, ball->Position.Y - closest.Y};
			auto dist = std::sqrt(away.X * away.X + away.Y * away.Y);
			if (dist < bestDist && dist > 0.0f)
			{
				bestDist = dist;
				bestNormal = {away.X / dist, away.Y / dist};
			}
		}
		if (bestDist > ball->Radius * 1.5f)
			continue;
		// The launch follows how far the plunger was pulled, from 45% of the table's strength for a quick tap to 100%
		// for a full pull. 45% still clears every FT2 shooter lane (the weakest that does: Captain Hero 35%, Alien
		// Daze and Mad Scientist 30%), so a short pull is a gentle shot (a skill shot), never a ball rolling back.
		auto pull = MaxPullback > 0 ? std::min(1.0f, Boost / MaxPullback) : 1.0f;
		auto speed = MaxPullback * (0.45f + 0.55f * pull) * (1.0f + RandFloat() * 0.03f);
		if (pb::DevDebug && getenv("FT2_PLUNGE"))
			speed = MaxPullback * static_cast<float>(atof(getenv("FT2_PLUNGE")));
		ball->Direction.X = bestNormal.X;
		ball->Direction.Y = bestNormal.Y;
		ball->Speed = speed;
		if (pb::DevDebug)
			fprintf(stderr, "FT2DBG plunger launch speed %.1f dir %.2f %.2f gap %.2f\n", speed, bestNormal.X, bestNormal.Y, bestDist);
	}
}

void TPlunger::Collision(TBall* ball, vector2* nextPosition, vector2* direction, float distance, TEdgeSegment* edge)
{
	if (PinballTable->TiltLockFlag || SomeCounter > 0) 
	{
		auto boost = RandFloat() * MaxPullback * 0.1f + MaxPullback;
		maths::basic_collision(ball, nextPosition, direction, Elasticity, Smoothness, 0, boost);
		if (SomeCounter)
			SomeCounter--;
		Message(MessageCode::PlungerInputReleased, 0.0);
	}
	else 
	{
		auto boost = RandFloat() * Boost * 0.1f + Boost;
		maths::basic_collision(ball, nextPosition, direction, Elasticity, Smoothness, Threshold, boost);
		// FT2 tables (10x Space Cadet's scale): a ball resting on the plunger bounced a little every frame, which showed
		// as jitter. While it is just resting, stop it dead.
		if (DirectLaunch() && Threshold >= 1e9f && ball->Speed < ball->Radius * 2.0f)
		{
			ball->Speed = 0.0f;
			ball->Direction.X = ball->Direction.Y = 0.0f;
		}
	}
}

int TPlunger::Message(MessageCode code, float value)
{
	if (pb::FT2Table && pb::DevDebug &&
		code != MessageCode::PlungerFeedBall)
		fprintf(stderr, "FT2DBG plunger msg %d boost %.1f started %d counter %d balls %d\n", static_cast<int>(code), Boost,
		        PullbackStartedFlag, SomeCounter, PinballTable->MultiballCount);
	switch (code)
	{
	case MessageCode::PlungerInputPressed:
		if (!PullbackStartedFlag && (!pb::FullTiltMode || PinballTable->MultiballCount > 0 && !PinballTable->
			TiltLockFlag))
		{
			PullbackStartedFlag = true;
			Boost = 0.0;
			Threshold = 1000000000.0;
			loader::play_sound(HardHitSoundId, this, "TPlunger1");
			PullbackTimer(0, this);
		}
		break;
	case MessageCode::PlungerFeedBall:
		{
			if (PinballTable->BallCountInRect(PinballTable->PlungerPosition, PinballTable->CollisionCompOffset * 1.2f))
			{
				timer::set(1.0f, this, BallFeedTimer);
			}
			else
			{
				auto ball = PinballTable->AddBall(PinballTable->PlungerPosition);
				assertm(ball, "Failure to create ball in plunger");
				PinballTable->MultiballCount++;
				PinballTable->BallInDrainFlag = 0;
				pb::tilt_no_more();
			}
			break;
		}
	case MessageCode::PlungerStartFeedTimer:
		timer::set(0.95999998f, this, BallFeedTimer);
		loader::play_sound(SoundIndexP1, this, "TPlunger2");
		break;
	case MessageCode::PlungerLaunchBall:
		PullbackStartedFlag = true;
		Boost = MaxPullback;
		Message(MessageCode::PlungerInputReleased, 0.0f);
		break;
	case MessageCode::PlungerRelaunchBall:
		SomeCounter++;
		timer::set(value, this, BallFeedTimer);
		loader::play_sound(SoundIndexP1, this, "TPlunger2_1");
		PullbackStartedFlag = true;
		PullbackTimer(0, this);
		break;
	case MessageCode::PlayerChanged:
		PullbackStartedFlag = false;
		Boost = 0.0f;
		Threshold = 1000000000.0f;
		SomeCounter = 0;
		timer::kill(BallFeedTimer);
		timer::kill(PullbackTimer);
		timer::kill(ReleasedTimer);
		break;
	case MessageCode::SetTiltLock:
		SomeCounter = 0;
		timer::kill(BallFeedTimer);
		break;
	case MessageCode::PlungerInputReleased:
	case MessageCode::Resume:
	case MessageCode::LooseFocus:
		if (PullbackStartedFlag && !SomeCounter)
		{
			PullbackStartedFlag = false;
			Threshold = 0.0;
			if (PullbackTimer_)
				timer::kill(PullbackTimer_);
			PullbackTimer_ = 0;
			loader::play_sound(SoundIndexP2, this, "TPlunger3");
			if (DirectLaunch())
				FT2Launch();
			SpriteSet(0);
			timer::set(PullbackDelay, this, ReleasedTimer);
		}
		break;
	case MessageCode::Reset:
	{
		PullbackStartedFlag = false;
		Boost = 0.0f;
		Threshold = 1000000000.0f;
		SomeCounter = 0;

		timer::kill(BallFeedTimer);
		timer::kill(PullbackTimer);
		timer::kill(ReleasedTimer);

		SpriteSet(0);
		break;
	}
	default:
		break;
	}

	control::handler(code, this);
	return 0;
}

void TPlunger::BallFeedTimer(int timerId, void* caller)
{
	auto plunger = static_cast<TPlunger*>(caller);
	plunger->Message(MessageCode::PlungerFeedBall, 0.0);
}

void TPlunger::PullbackTimer(int timerId, void* caller)
{
	auto plunger = static_cast<TPlunger*>(caller);
	plunger->Boost += plunger->PullbackIncrement;
	if (plunger->Boost <= plunger->MaxPullback)
	{
		if (plunger->SomeCounter)
		{
			plunger->PullbackTimer_ = timer::set(plunger->PullbackDelay / 4.0f, plunger, PullbackTimer);
		}
		else
		{
			plunger->PullbackTimer_ = timer::set(plunger->PullbackDelay, plunger, PullbackTimer);
		}
	}
	else
	{
		plunger->PullbackTimer_ = 0;
		plunger->Boost = plunger->MaxPullback;
	}

	if (!plunger->ListBitmap) // launchers without a plunger picture (Skulduggery's cannon)
		return;
	int index = static_cast<int>(floor(
		static_cast<float>(plunger->ListBitmap->size() - 1) *
		(plunger->Boost / plunger->MaxPullback)));
	plunger->SpriteSet(index);
}

void TPlunger::ReleasedTimer(int timerId, void* caller)
{
	auto plunger = static_cast<TPlunger*>(caller);
	plunger->Threshold = 1000000000.0;
	plunger->Boost = 0.0;
}
