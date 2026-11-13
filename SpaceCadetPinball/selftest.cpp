#include "pch.h"
#include "selftest.h"

#include "maths.h"
#include "pb.h"
#include "TBall.h"
#include "TFlipper.h"
#include "TFlipperEdge.h"
#include "TPinballTable.h"
#include "winmain.h"

namespace
{
	float Seconds = 0.0f; // 0 = off
	float BallLimit = 240.0f;
	float Elapsed = 0.0f;
	int ExitCodeValue = 0;
	bool Finished = false;

	int Games = 0, Balls = 0, Launches = 0;
	float BallTime = 0.0f, LongestBall = 0.0f;
	bool BallInPlay = false;
	std::string Failure;

	// Plunger: wait, pull for a random time, release
	float PlungerWait = 0.0f, PlungerHold = -1.0f;
	// Flippers: hold time left and cool-down per side (0 left, 1 right)
	float FlipHold[2]{}, FlipCool[2]{};
	float GameOverWait = 0.0f;
	float StillTime = 0.0f;
	vector2 StillPos{};

	float distance(const vector2& a, const vector2& b)
	{
		return std::sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));
	}

	bool ball_near(const std::vector<TFlipper*>& flippers, TPinballTable* table)
	{
		for (auto flipper : flippers)
		{
			auto edge = flipper->FlipperEdge;
			if (!edge)
				continue;
			auto reach = distance(edge->RotOrigin, edge->T1Src) + edge->CircleT1Radius;
			for (auto ball : table->BallList)
				if (ball->ActiveFlag && distance(ball->Position, edge->RotOrigin) < reach + ball->Radius * 3.0f)
					return true;
		}
		return false;
	}

	void finish(const std::string& failure)
	{
		if (Finished)
			return;
		Finished = true;
		if (!failure.empty())
			Failure = failure;
		ExitCodeValue = Failure.empty() ? 0 : 1;
		char line[512];
		snprintf(line, sizeof line,
		         "SELFTEST table=%s seconds=%.0f games=%d balls=%d launches=%d longest_ball=%.1f result=%s%s%s\n",
		         pb::DatFileName.c_str(), Elapsed, Games, Balls, Launches, LongestBall,
		         Failure.empty() ? "PASS" : "FAIL", Failure.empty() ? "" : " reason=", Failure.c_str());
		printf("%s", line);
		fprintf(stderr, "%s", line);
		fflush(stdout);
		SDL_Event quit{SDL_QUIT};
		SDL_PushEvent(&quit);
	}
}

bool selftest::Active()
{
	return Seconds > 0.0f;
}

void selftest::Parse(const std::string& commandLine)
{
	auto pos = commandLine.find("--selftest ");
	if (pos != std::string::npos)
		Seconds = static_cast<float>(atof(commandLine.c_str() + pos + 11));
	pos = commandLine.find("--selftest-ball-limit ");
	if (pos != std::string::npos)
		BallLimit = static_cast<float>(atof(commandLine.c_str() + pos + 22));
}

int selftest::ExitCode()
{
	return ExitCodeValue;
}

void selftest::Frame(float dtMilliSec)
{
	auto table = pb::MainTable;
	if (!Active() || Finished || !table)
		return;
	auto dt = dtMilliSec / 1000.0f;
	Elapsed += dt;

	// A new game at the start and after every game over (the engine starts the first one by itself)
	static bool wasInGame = false;
	if (pb::game_mode == GameModes::InGame && !wasInGame)
		Games++;
	wasInGame = pb::game_mode == GameModes::InGame;
	if (pb::game_mode != GameModes::InGame)
	{
		GameOverWait += dt;
		if (GameOverWait > 2.0f)
		{
			GameOverWait = 0.0f;
			pb::replay_level(false);
		}
		if (Elapsed >= Seconds)
			finish("");
		return;
	}

	// FT2_SHOT="x y dirX dirY speed mask delay" (development): once, after delay seconds of play, fire the ball from
	// that spot, to test one shot (a ramp, an orbit) without waiting for the self-test player to make it
	static bool shotDone = false;
	if (!shotDone && getenv("FT2_SHOT"))
	{
		float x, y, dx, dy, speed, delay;
		int mask;
		if (sscanf(getenv("FT2_SHOT"), "%f %f %f %f %f %d %f", &x, &y, &dx, &dy, &speed, &mask, &delay) == 7 &&
			Elapsed >= delay)
			for (auto ball : table->BallList)
				if (ball->ActiveFlag && !ball->CollisionComp)
				{
					shotDone = true;
					auto len = std::sqrt(dx * dx + dy * dy);
					ball->Position = {x, y, ball->Radius};
					ball->Direction = {dx / len, dy / len, 0};
					ball->Speed = speed;
					ball->CollisionMask = mask;
					ball->CollisionFlag = 0;
					fprintf(stderr, "SHOT fired %.1f %.1f\n", x, y);
					break;
				}
	}
	if (getenv("FT2_SHOT") && shotDone)
	{
		static int traceFrame = 0;
		for (auto ball : table->BallList)
			if (ball->ActiveFlag && ++traceFrame % 6 == 0)
				fprintf(stderr, "SHOT ball %.1f %.1f mask %d\n", ball->Position.X, ball->Position.Y, ball->CollisionMask);
	}

	bool anyActive = false;
	TBall* moving = nullptr;
	for (auto ball : table->BallList)
		if (ball->ActiveFlag)
		{
			anyActive = true;
			moving = ball;
		}
	// At the plunger: near its spot, or anywhere in the lane straight below/above it (Skulduggery's cannon ball rests
	// a few ball widths down the lane from the plunger's own position)
	bool atPlunger = moving && moving->Speed < moving->Radius &&
		(distance(moving->Position, table->PlungerPosition) < moving->Radius * 4.0f ||
		 (std::fabs(moving->Position.X - table->PlungerPosition.X) < moving->Radius * 2.0f &&
		  std::fabs(moving->Position.Y - table->PlungerPosition.Y) < moving->Radius * 15.0f));

	// Ball accounting: a ball is "in play" from its launch until no ball is left on the table
	if (anyActive && !atPlunger && !BallInPlay)
	{
		BallInPlay = true;
		BallTime = 0.0f;
	}
	if (BallInPlay)
	{
		BallTime += dt;
		LongestBall = std::max(LongestBall, BallTime);
		if (!anyActive)
		{
			BallInPlay = false;
			Balls++;
		}
		else if (BallTime > BallLimit)
		{
			finish("ball in play over " + std::to_string(static_cast<int>(BallLimit)) + " s (never drains?)");
			return;
		}
	}

	// A ball sitting still away from the plunger (stuck on something)
	if (moving && !atPlunger && moving->Speed < 0.05f * moving->Radius && distance(moving->Position, StillPos) < moving->Radius)
	{
		StillTime += dt;
		if (StillTime > 25.0f)
		{
			char where[96];
			snprintf(where, sizeof where, "ball still for 25 s at %.1f,%.1f", moving->Position.X, moving->Position.Y);
			finish(where);
			return;
		}
	}
	else
	{
		StillTime = 0.0f;
		if (moving)
			StillPos = moving->Position;
	}

	// Plunger: pull for a random 0.1-1.6 s once the ball rests on it
	if (PlungerHold >= 0.0f)
	{
		PlungerHold -= dt;
		if (PlungerHold < 0.0f)
		{
			table->Message(MessageCode::PlungerInputReleased, pb::time_now);
			Launches++;
			PlungerWait = 0.0f;
		}
	}
	else if (atPlunger || !anyActive)
	{
		PlungerWait += dt;
		if (PlungerWait > 1.0f)
		{
			table->Message(MessageCode::PlungerInputPressed, pb::time_now);
			PlungerHold = 0.1f + RandFloat() * 1.5f;
		}
	}

	// Flippers: flip a side when a ball comes near one of its flippers
	const std::vector<TFlipper*>* sides[2] = {&table->FlippersL, &table->FlippersR};
	for (int side = 0; side < 2; side++)
	{
		if (FlipHold[side] > 0.0f)
		{
			FlipHold[side] -= dt;
			if (FlipHold[side] <= 0.0f)
			{
				table->Message(side ? MessageCode::RightFlipperInputReleased : MessageCode::LeftFlipperInputReleased,
				               pb::time_now);
				FlipCool[side] = 0.25f;
			}
			continue;
		}
		if (FlipCool[side] > 0.0f)
		{
			FlipCool[side] -= dt;
			continue;
		}
		if (ball_near(*sides[side], table))
		{
			table->Message(side ? MessageCode::RightFlipperInputPressed : MessageCode::LeftFlipperInputPressed,
			               pb::time_now);
			FlipHold[side] = 0.18f;
		}
	}

	if (Elapsed >= Seconds)
	{
		if (Launches == 0)
			finish("no ball was ever launched");
		else
			finish("");
	}
}
