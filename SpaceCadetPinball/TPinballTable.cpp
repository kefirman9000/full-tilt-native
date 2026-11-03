#include "pch.h"
#include "TPinballTable.h"


#include "control.h"
#include "loader.h"
#include "GroupData.h"
#include "fullscrn.h"
#include "midi.h"
#include "pb.h"
#include "render.h"
#include "TBall.h"
#include "ft2_animation.h"
#include "ft2_parts.h"
#include "TBlocker.h"
#include "TBumper.h"
#include "TComponentGroup.h"
#include "TDemo.h"
#include "TFlagSpinner.h"
#include "TGate.h"
#include "THole.h"
#include "timer.h"
#include "TKickback.h"
#include "TKickout.h"
#include "TLight.h"
#include "TLightBargraph.h"
#include "TLightGroup.h"
#include "TLightRollover.h"
#include "TOneway.h"
#include "TPopupTarget.h"
#include "TRamp.h"
#include "TRollover.h"
#include "TSink.h"
#include "TSoloTarget.h"
#include "TSound.h"
#include "TTableLayer.h"
#include "TTextBox.h"
#include "TTimer.h"
#include "TTripwire.h"
#include "TWall.h"
#include "TPlunger.h"
#include "TFlipper.h"
#include "TDrain.h"
#include "translations.h"

int TPinballTable::score_multipliers[5] = {1, 2, 3, 5, 10};


TPinballTable::TPinballTable(): TPinballComponent(nullptr, -1, false)
{
	int shortArrLength;

	CurScoreStruct = nullptr;
	ScoreBallcount = nullptr;
	ScorePlayerNumber1 = nullptr;
	BallInDrainFlag = 0;
	ActiveFlag = 1;
	TiltLockFlag = 0;
	EndGameTimeoutTimer = 0;
	LightShowTimer = 0;
	ReplayTimer = 0;
	TiltTimeoutTimer = 0;
	MultiballFlag = false;
	PlayerCount = 0;

	auto ball = AddBall({0.0f, 0.0f});
	ball->Disable();

	new TTableLayer(this);
	LightGroup = new TLightGroup(this, 0);

	auto score1 = score::create("score1", render::background_bitmap);
	CurScoreStruct = score1;
	PlayerScores[0].ScoreStruct = score1;
	for (int scoreIndex = 1; scoreIndex < 4; scoreIndex++)
	{
		PlayerScores[scoreIndex].ScoreStruct = score::dup(CurScoreStruct, scoreIndex);
	}

	CurrentPlayer = 0;
	MaxBallCount = 3;
	ScoreBallcount = score::create("ballcount1", render::background_bitmap);
	ScorePlayerNumber1 = score::create("player_number1", render::background_bitmap);
	int groupIndexObjects = loader::query_handle("table_objects");

	short* shortArr = loader::query_iattribute(groupIndexObjects, 1025, &shortArrLength);
	if (shortArrLength > 0)
	{
		for (int i = 0; i < shortArrLength / 2; ++i)
		{
			int objectType = *shortArr;
			short* shortArrPlus1 = shortArr + 1;
			int groupIndex = *shortArrPlus1;
			shortArr = shortArrPlus1 + 1;
			switch (objectType)
			{
			case 1000:
			case 1010:
				new TWall(this, groupIndex);
				break;
			case 1001:
				Plunger = new TPlunger(this, groupIndex);
				break;
			case 1002:
				LightGroup->List.push_back(new TLight(this, groupIndex));
				break;
			case 1003:
				FlipperL = new TFlipper(this, groupIndex);
				FlippersL.push_back(FlipperL);
				break;
			case 1004:
				FlipperR = new TFlipper(this, groupIndex);
				FlippersR.push_back(FlipperR);
				break;
			case 1005:
				new TBumper(this, groupIndex);
				break;
			case 1006:
				new TPopupTarget(this, groupIndex);
				break;
			case 1007:
				Drain = new TDrain(this, groupIndex);
				break;
			case 1011:
				new TBlocker(this, groupIndex);
				break;
			case 1012:
				new TKickout(this, groupIndex, true);
				break;
			case 1013:
				new TGate(this, groupIndex);
				break;
			case 1014:
				new TKickback(this, groupIndex);
				break;
			case 1015:
				new TRollover(this, groupIndex);
				break;
			case 1016:
				new TOneway(this, groupIndex);
				break;
			case 1017:
				new TSink(this, groupIndex);
				break;
			case 1018:
				new TFlagSpinner(this, groupIndex);
				break;
			case 1019:
				new TSoloTarget(this, groupIndex);
				break;
			case 1020:
				new TLightRollover(this, groupIndex);
				break;
			case 1021:
				new TRamp(this, groupIndex);
				break;
			case 1022:
				new THole(this, groupIndex);
				break;
			case 1023:
				new TDemo(this, groupIndex);
				break;
			case 1024:
				new TTripwire(this, groupIndex);
				break;
			case 1026:
				new TLightGroup(this, groupIndex);
				break;
			case 1028:
				new TComponentGroup(this, groupIndex);
				break;
			case 1029:
				new TKickout(this, groupIndex, false);
				break;
			case 1030:
				new TLightBargraph(this, groupIndex);
				break;
			case 1031:
				new TSound(this, groupIndex);
				break;
			case 1032:
				new TTimer(this, groupIndex);
				break;
			case 1033:
				new TTextBox(this, groupIndex);
				break;
			case 1035:
				if (pb::FT2Table)
					new TFT2RampExit(this, groupIndex);
				break;
			case 1036:
				if (pb::FT2Table)
					new TWall(this, groupIndex); // accelerator: a wall with a kick
				break;
			case 1043:
				if (pb::FT2Table)
					new TFT2Diverter(this, groupIndex);
				break;
			case 1044:
				if (pb::FT2Table)
					new TFT2Animation(this, groupIndex);
				break;
			case 1045:
				if (pb::FT2Table)
					TFT2Animation::AddFrameObject(this, groupIndex);
				break;
			case 1034:
				// Full Tilt launcher without a plunger picture (Skulduggery's cannon, s_launcher1): works as the plunger
				if (pb::FT2Table && !Plunger)
					Plunger = new TPlunger(this, groupIndex);
				break;
			default:
				continue;
			}
		}
	}

	if (pb::FT2Table)
	{
		// FT2 sandbox (no table rules yet): some gates are two one-ways laid back to back in opposite directions, and
		// the table's rules switch one of them off. With both on, the ball can pass neither way (e.g. Alien's plunger
		// lane exit). Until the rules exist, keep the first of each such pair and switch the later one off.
		std::vector<TOneway*> oneways;
		for (auto comp : ComponentList)
			if (auto oneway = dynamic_cast<TOneway*>(comp))
				oneways.push_back(oneway);
		auto close = [](vector2 a, vector2 b) { return std::abs(a.X - b.X) < 1.5f && std::abs(a.Y - b.Y) < 1.5f; };
		for (size_t i = 0; i < oneways.size(); i++)
			for (size_t j = i + 1; j < oneways.size(); j++)
				if (oneways[i]->ActiveFlag && oneways[j]->ActiveFlag &&
					close(oneways[i]->Pt0, oneways[j]->Pt1) && close(oneways[i]->Pt1, oneways[j]->Pt0))
				{
					oneways[j]->ActiveFlag = 0;
					printf("FT2: one-way %s off (back to back with %s)\n", oneways[j]->GroupName, oneways[i]->GroupName);
				}
	}

	render::build_occlude_list();
	pb::InfoTextBox = pb::FT2FullScreen ? nullptr : dynamic_cast<TTextBox*>(find_component("info_text_box"));
	pb::MissTextBox = pb::FT2FullScreen ? nullptr : dynamic_cast<TTextBox*>(find_component("mission_text_box"));
	// FT2 tables have no text box groups (text is drawn by the table's own code): use zero-size dummies
	if (pb::FT2Table)
	{
		// Place them in the table's own message areas (records: font, x, y, w, h, align; one per resolution)
		auto makeBox = [this](std::initializer_list<const char*> recordNames)
		{
			// Tables name their message areas differently (Mad Scientist has no notify_box): first one found wins
			auto box = new TTextBox(this, -1);
			int groupIndex = -1;
			for (auto recordName : recordNames)
				if ((groupIndex = loader::query_handle(recordName)) > 0)
					break;
			if (groupIndex > 0)
			{
				auto resGroup = groupIndex + fullscrn::GetResolution();
				auto dims = reinterpret_cast<int16_t*>(loader::loader_table->field(resGroup, FieldTypes::ShortArray));
				int arrLength = dims ? loader::loader_table->field_size(resGroup, FieldTypes::ShortArray) / 2 : 0;
				if (dims && arrLength >= 5)
				{
					box->OffsetX = dims[1];
					box->OffsetY = dims[2];
					box->Width = dims[3];
					box->Height = dims[4] * 2;
				}
			}
			return box;
		};
		if (!pb::InfoTextBox)
			pb::InfoTextBox = pb::FT2FullScreen ? makeBox({"notify_box", "time_box", "jackpot_box"}) : new TTextBox(this, -1);
		if (!pb::MissTextBox)
			pb::MissTextBox = pb::FT2FullScreen ? makeBox({"mode_box", "brain_box", "super_jackpot_box", "jackpot_box"}) : pb::InfoTextBox;
	}
	control::make_links(this);
}


TPinballTable::~TPinballTable()
{
	for (auto& PlayerScore : PlayerScores)
	{
		delete PlayerScore.ScoreStruct;
	}
	if (ScorePlayerNumber1)
	{
		delete ScorePlayerNumber1;
		ScorePlayerNumber1 = nullptr;
	}
	if (ScoreBallcount)
	{
		delete ScoreBallcount;
		ScoreBallcount = nullptr;
	}
	delete LightGroup;
	while (!ComponentList.empty())
	{
		// Component destructor removes it from the list.
		delete ComponentList[0];
	}
	control::ClearLinks();
	pb::InfoTextBox = pb::MissTextBox = nullptr;
}

TPinballComponent* TPinballTable::find_component(LPCSTR componentName)
{
	for (auto component : ComponentList)
	{
		const char* groupName = component->GroupName;
		if (groupName && !strcmp(groupName, componentName))
		{
			return component;
		}
	}

	pb::ShowMessageBox(SDL_MESSAGEBOX_WARNING, "Table cant find:", componentName);
	return nullptr;
}

TPinballComponent* TPinballTable::find_component(int groupIndex)
{
	char Buffer[40]{};
	for (auto component : ComponentList)
	{
		if (component->GroupIndex == groupIndex)
			return component;
	}

	snprintf(Buffer, sizeof Buffer, "%d", groupIndex);
	pb::ShowMessageBox(SDL_MESSAGEBOX_WARNING, "Table cant find (lh):", Buffer);
	return nullptr;
}

int TPinballTable::AddScore(int score)
{
	if (JackpotScoreFlag)
	{
		JackpotScore += score;
		const auto jackpotLimit = !pb::FullTiltMode ? 5000000 : 10000000;
		if (JackpotScore > jackpotLimit)
			JackpotScore = jackpotLimit;
	}
	if (BonusScoreFlag)
	{
		BonusScore += score;
		if (BonusScore > 5000000)
			BonusScore = 5000000;
	}
	int addedScore = ScoreAdded + score * score_multipliers[ScoreMultiplier];
	CurScore += addedScore;
	if (CurScore > 1000000000)
	{
		++CurScoreE9;
		CurScore = CurScore - 1000000000;
	}
	score::set(CurScoreStruct, CurScore);
	return addedScore;
}

void TPinballTable::ChangeBallCount(int count)
{
	BallCount = count;
	if (count <= 0)
	{
		score::erase(ScoreBallcount, 1);
	}
	else
	{
		score::set(ScoreBallcount, MaxBallCount - count + 1);
		score::update(ScoreBallcount);
	}
}

void TPinballTable::tilt(float time)
{
	if (!TiltLockFlag && !BallInDrainFlag)
	{
		pb::InfoTextBox->Clear();
		pb::MissTextBox->Clear();
		pb::InfoTextBox->Display(pb::get_rc_string(Msg::STRING136), -1.0);
		loader::play_sound(SoundIndex3, nullptr, "TPinballTable1");
		TiltTimeoutTimer = timer::set(30.0, this, tilt_timeout);

		for (auto component : ComponentList)
		{
			component->Message(MessageCode::SetTiltLock, time);
		}
		LightGroup->Message(MessageCode::TLightTurnOffTimed, 0);
		TiltLockFlag = 1;
		control::table_control_handler(MessageCode::SetTiltLock);
	}
}


void TPinballTable::port_draw()
{
	for (auto component : ComponentList)
	{
		component->port_draw();
	}
}

int TPinballTable::Message(MessageCode code, float value)
{
	const char* rc_text;

	switch (code)
	{
	case MessageCode::LeftFlipperInputPressed:
		if (!TiltLockFlag)
		{
			for (auto flipper : FlippersL)
				flipper->Message(MessageCode::TFlipperExtend, value);
		}
		break;
	case MessageCode::LeftFlipperInputReleased:
		if (!TiltLockFlag)
		{
			for (auto flipper : FlippersL)
				flipper->Message(MessageCode::TFlipperRetract, value);
		}
		break;
	case MessageCode::RightFlipperInputPressed:
		if (!TiltLockFlag)
		{
			for (auto flipper : FlippersR)
				flipper->Message(MessageCode::TFlipperExtend, value);
		}
		break;
	case MessageCode::RightFlipperInputReleased:
		if (!TiltLockFlag)
		{
			for (auto flipper : FlippersR)
				flipper->Message(MessageCode::TFlipperRetract, value);
		}
		break;
	case MessageCode::PlungerInputPressed:
	case MessageCode::PlungerInputReleased:
		if (Plunger)
			Plunger->Message(code, value);
		break;
	case MessageCode::Pause:
	case MessageCode::Resume:
	case MessageCode::LooseFocus:
		for (auto component : ComponentList)
		{
			component->Message(code, value);
		}
		break;
	case MessageCode::ClearTiltLock:
		LightGroup->Message(MessageCode::TLightResetTimed, 0.0);
		if (TiltLockFlag)
		{
			TiltLockFlag = 0;
			if (TiltTimeoutTimer)
				timer::kill(TiltTimeoutTimer);
			TiltTimeoutTimer = 0;
		}
		break;
	case MessageCode::StartGamePlayer1:
		LightGroup->Message(MessageCode::TLightGroupReset, 0.0);
		LightGroup->Message(MessageCode::TLightResetAndTurnOff, 0.0);
		if (Plunger)
			Plunger->Message(MessageCode::PlungerStartFeedTimer, 0.0);
		if (Demo && Demo->ActiveFlag)
			rc_text = pb::get_rc_string(Msg::STRING131);
		else
			rc_text = pb::get_rc_string(Msg::STRING127);
		pb::InfoTextBox->Display(rc_text, -1.0);
		if (Demo)
			Demo->Message(MessageCode::NewGame, 0.0);
		break;
	case MessageCode::NewGame:
		if (EndGameTimeoutTimer)
		{
			timer::kill(EndGameTimeoutTimer);
			EndGame_timeout(0, this);
			pb::mode_change(GameModes::InGame);
		}
		if (LightShowTimer)
		{
			timer::kill(LightShowTimer);
			LightShowTimer = 0;
			Message(MessageCode::StartGamePlayer1, 0.0);
		}
		else
		{
			// Some of the control cheats persist across games.
			// Was this loose anti-cheat by design?
			CheatsUsed = 0;
			Message(MessageCode::Reset, 0.0);
			auto ball = BallList[0];
			ball->Position.Y = 0.0;
			ball->Position.X = 0.0;
			ball->Position.Z = -0.8f;

			auto playerCount = static_cast<int>(floor(value));
			PlayerCount = playerCount;
			if (playerCount >= 1)
			{
				if (playerCount > 4)
					PlayerCount = 4;
			}
			else
			{
				PlayerCount = 1;
			}

			auto plr1Score = PlayerScores[0].ScoreStruct;
			CurrentPlayer = 0;
			CurScoreStruct = plr1Score;
			CurScore = 0;
			score::set(plr1Score, 0);
			ScoreMultiplier = 0;

			for (int plrIndex = 1; plrIndex < PlayerCount; ++plrIndex)
			{
				auto scorePtr = &PlayerScores[plrIndex];
				score::set(scorePtr->ScoreStruct, 0);
				scorePtr->Score = 0;
				scorePtr->ScoreE9Part = 0;
				scorePtr->BallCount = MaxBallCount;
				scorePtr->ExtraBalls = ExtraBalls;
				scorePtr->BallLockedCounter = BallLockedCounter;
				scorePtr->JackpotScore = JackpotScore;
			}

			BallCount = MaxBallCount;
			ChangeBallCount(BallCount);
			score::set(ScorePlayerNumber1, CurrentPlayer + 1);
			score::update(ScorePlayerNumber1);

			for (auto scoreIndex = 4 - PlayerCount; scoreIndex > 0; scoreIndex--)
			{
				score::set(PlayerScores[scoreIndex].ScoreStruct, -1);
			}

			JackpotScoreFlag = false;
			BonusScoreFlag = false;
			UnknownP71 = 0;
			pb::InfoTextBox->Clear();
			pb::MissTextBox->Clear();
			LightGroup->Message(MessageCode::TLightGroupLightShowAnimation, 0.2f);
			auto time = loader::play_sound(SoundIndex1, nullptr, "TPinballTable2");
			if (time < 0)
				time = 5.0f;
			LightShowTimer = timer::set(time, this, LightShow_timeout);
		}

		if (pb::FullTiltMode)
		{
			// Multi-ball is FT exclusive feature, at least for now.
			MultiballFlag = true;
			JackpotScore = 500000;
		}
		midi::play_track(MidiTracks::Track1, true);
		break;
	case MessageCode::PlungerRelaunchBall:
		if (ReplayTimer)
			timer::kill(ReplayTimer);
		ReplayTimer = timer::set(floor(value), this, replay_timer_callback);
		ReplayActiveFlag = 1;
		break;
	case MessageCode::SwitchToNextPlayer:
		{
			if (PlayerCount <= 1)
			{
				const char* textboxText;
				if (Demo->ActiveFlag)
					textboxText = pb::get_rc_string(Msg::STRING131);
				else
					textboxText = pb::get_rc_string(Msg::STRING127);
				pb::InfoTextBox->Display(textboxText, -1.0);
				break;
			}

			auto nextPlayer = (CurrentPlayer + 1) % PlayerCount;
			auto nextScorePtr = &PlayerScores[nextPlayer];
			if (nextScorePtr->BallCount <= 0)
				break;

			PlayerScores[CurrentPlayer].Score = CurScore;
			PlayerScores[CurrentPlayer].ScoreE9Part = CurScoreE9;
			PlayerScores[CurrentPlayer].BallCount = BallCount;
			PlayerScores[CurrentPlayer].ExtraBalls = ExtraBalls;
			PlayerScores[CurrentPlayer].BallLockedCounter = BallLockedCounter;
			PlayerScores[CurrentPlayer].JackpotScore = JackpotScore;

			CurScore = nextScorePtr->Score;
			CurScoreE9 = nextScorePtr->ScoreE9Part;
			BallCount = nextScorePtr->BallCount;
			ExtraBalls = nextScorePtr->ExtraBalls;
			BallLockedCounter = nextScorePtr->BallLockedCounter;
			JackpotScore = nextScorePtr->JackpotScore;

			CurScoreStruct = nextScorePtr->ScoreStruct;
			score::set(CurScoreStruct, CurScore);
			CurScoreStruct->DirtyFlag = true;

			ChangeBallCount(BallCount);
			score::set(ScorePlayerNumber1, nextPlayer + 1);
			score::update(ScorePlayerNumber1);

			for (auto component : ComponentList)
			{
				component->Message(MessageCode::PlayerChanged, static_cast<float>(nextPlayer));
			}

			const char* textboxText = nullptr;
			switch (nextPlayer)
			{
			case 0:
				if (Demo->ActiveFlag)
					textboxText = pb::get_rc_string(Msg::STRING131);
				else
					textboxText = pb::get_rc_string(Msg::STRING127);
				break;
			case 1:
				if (Demo->ActiveFlag)
					textboxText = pb::get_rc_string(Msg::STRING132);
				else
					textboxText = pb::get_rc_string(Msg::STRING128);
				break;
			case 2:
				if (Demo->ActiveFlag)
					textboxText = pb::get_rc_string(Msg::STRING133);
				else
					textboxText = pb::get_rc_string(Msg::STRING129);
				break;
			case 3:
				if (Demo->ActiveFlag)
					textboxText = pb::get_rc_string(Msg::STRING134);
				else
					textboxText = pb::get_rc_string(Msg::STRING130);
				break;
			default:
				break;
			}

			if (textboxText != nullptr)
				pb::InfoTextBox->Display(textboxText, -1);
			JackpotScoreFlag = false;
			BonusScoreFlag = false;
			UnknownP71 = 0;
			CurrentPlayer = nextPlayer;
		}
		break;
	case MessageCode::GameOver:
		loader::play_sound(SoundIndex2, nullptr, "TPinballTable3");
		pb::MissTextBox->Clear();
		pb::InfoTextBox->Display(pb::get_rc_string(Msg::STRING135), -1.0);
		EndGameTimeoutTimer = timer::set(3.0, this, EndGame_timeout);
		break;
	case MessageCode::Reset:
		for (auto component : ComponentList)
		{
			component->Message(MessageCode::Reset, 0);
		}
		if (ReplayTimer)
			timer::kill(ReplayTimer);
		ReplayTimer = 0;
		if (LightShowTimer)
		{
			timer::kill(LightShowTimer);
			LightGroup->Message(MessageCode::TLightGroupReset, 0.0);
		}
		LightShowTimer = 0;
		ScoreMultiplier = 0;
		ScoreAdded = 0;
		ReflexShotScore = 0;
		BonusScore = 10000;
		BonusScoreFlag = false;
		JackpotScore = 20000;
		JackpotScoreFlag = false;
		UnknownP71 = 0;
		ExtraBalls = 0;
		MultiballCount = 0;
		BallLockedCounter = 0;
		MultiballFlag = false;
		UnknownP78 = 0;
		ReplayActiveFlag = 0;
		ReplayTimer = 0;
		TiltLockFlag = 0;
		break;

	default: break;
	}

	control::table_control_handler(code);
	return 0;
}

TBall* TPinballTable::AddBall(vector2 position)
{
	if (pb::FT2Table && pb::DevDebug)
		fprintf(stderr, "FT2DBG AddBall %.2f %.2f\n", position.X, position.Y);
	TBall* ball = nullptr;

	for (auto curBall : BallList)
	{
		if (!curBall->ActiveFlag)
		{
			ball = curBall;
			break;
		}
	}

	if (ball != nullptr)
	{
		ball->ActiveFlag = 1;
		ball->Position.Z = ball->Radius;
		ball->Direction = {};
		ball->Speed = 0;
		ball->TimeDelta = 0;
		ball->EdgeCollisionCount = 0;
		ball->CollisionFlag = 0;
		ball->CollisionMask = 1;
		ball->CollisionComp = nullptr;
	}
	else
	{
		if (BallList.size() >= 20)
			return nullptr;
		ball = new TBall(this, -1);
		BallList.push_back(ball);
	}

	ball->Position.X = position.X;
	ball->Position.Y = position.Y;
	ball->PrevPosition = ball->Position;
	ball->StuckCounter = 0;
	ball->LastActiveTime = pb::time_ticks;

	return ball;
}

int TPinballTable::BallCountInRect(const RectF& rect)
{
	int count = 0;
	for (const auto ball : BallList)
	{
		if (ball->ActiveFlag &&
			ball->Position.X >= rect.XMin &&
			ball->Position.Y >= rect.YMin &&
			ball->Position.X <= rect.XMax &&
			ball->Position.Y <= rect.YMax)
		{
			count++;
		}
	}
	return count;
}

int TPinballTable::BallCountInRect(const vector2& pos, float margin)
{
	RectF rect{};
	rect.XMin = pos.X - margin;
	rect.XMax = pos.X + margin;
	rect.YMin = pos.Y - margin;
	rect.YMax = pos.Y + margin;
	return BallCountInRect(rect);
}

void TPinballTable::EndGame_timeout(int timerId, void* caller)
{
	auto table = static_cast<TPinballTable*>(caller);
	table->EndGameTimeoutTimer = 0;
	pb::end_game();

	for (auto component : table->ComponentList)
	{
		component->Message(MessageCode::GameOver, 0);
	}
	if (table->Demo)
		table->Demo->Message(MessageCode::GameOver, 0.0);
	control::handler(MessageCode::ControlMissionStarted, pb::MissTextBox);
	pb::InfoTextBox->Display(pb::get_rc_string(Msg::STRING125), -1.0);
}

void TPinballTable::LightShow_timeout(int timerId, void* caller)
{
	auto table = static_cast<TPinballTable*>(caller);
	table->LightShowTimer = 0;
	table->Message(MessageCode::StartGamePlayer1, 0.0);
}

void TPinballTable::replay_timer_callback(int timerId, void* caller)
{
	auto table = static_cast<TPinballTable*>(caller);
	table->ReplayActiveFlag = 0;
	table->ReplayTimer = 0;
}

void TPinballTable::tilt_timeout(int timerId, void* caller)
{
	auto table = static_cast<TPinballTable*>(caller);
	vector2 vec{};

	table->TiltTimeoutTimer = 0;
	if (table->TiltLockFlag)
	{
		for (auto ball : table->BallList)
		{
			table->Drain->Collision(ball, &vec, &vec, 0.0, nullptr);
		}
	}
}
