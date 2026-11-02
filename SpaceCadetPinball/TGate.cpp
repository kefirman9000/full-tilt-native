#include "pch.h"
#include "pb.h"
#include "TGate.h"


#include "control.h"
#include "loader.h"
#include "render.h"

TGate::TGate(TPinballTable* table, int groupIndex) : TCollisionComponent(table, groupIndex, true)
{
	visualStruct visual{};

	loader::query_visual(groupIndex, 0, &visual);
	SoundIndex4 = visual.SoundIndex4;
	SoundIndex3 = visual.SoundIndex3;
	ActiveFlag = 1;
	SpriteSet(0);
	Message(MessageCode::Reset, 0.0f);
	control::handler(MessageCode::Reset, this);
}

int TGate::Message(MessageCode code, float value)
{
	switch (code)
	{
	case MessageCode::TGateDisable:
		ActiveFlag = 0;
		SpriteSet(-1);
		loader::play_sound(SoundIndex3, this, "TGate1");
		break;
	case MessageCode::Reset:
		// FT2 sandbox: diverter pairs ("..._launch" / "..._return", Captain Hero's shooter ramp) are switched by the table
		// rules; with both up the shot can't pass. Until the rules exist the "return" side starts down.
		if (pb::FT2Table && GroupName && strstr(GroupName, "_return"))
		{
			ActiveFlag = 0;
			SpriteSet(-1);
			break;
		}
		ActiveFlag = 1;
		SpriteSet(0);
		break;
	case MessageCode::TGateEnable:
		ActiveFlag = 1;
		SpriteSet(0);
		if (code == MessageCode::TGateEnable)
			loader::play_sound(SoundIndex4, this, "TGate2");
		break;
	default: break;
	}

	control::handler(code, this);
	return 0;
}
