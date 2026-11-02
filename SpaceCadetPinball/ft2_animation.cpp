#include "pch.h"
#include "ft2_animation.h"

#include "fullscrn.h"
#include "loader.h"
#include "maths.h"
#include "proj.h"
#include "render.h"
#include "timer.h"
#include "TPinballTable.h"

static std::string FrameBaseName(const char* name)
{
	std::string base = name ? name : "";
	while (!base.empty() && (isdigit(static_cast<unsigned char>(base.back())) || base.back() == '_'))
		base.pop_back();
	return base;
}

TFT2Animation::TFT2Animation(TPinballTable* table, int groupIndex) : TPinballComponent(table, groupIndex, false)
{
	BaseName = FrameBaseName(GroupName);
	auto pos = loader::query_float_attribute(groupIndex, 0, 700 + fullscrn::GetResolution());
	if (pos)
	{
		X = static_cast<int>(pos[0]);
		Y = static_cast<int>(pos[1]);
	}
	auto where = reinterpret_cast<vector3*>(loader::query_float_attribute(groupIndex, 0, 501));
	// (0, 0, z) marks a flat overlay (drawn on top); otherwise the 3D point gives its depth
	if (where && (where->X != 0.0f || where->Y != 0.0f))
		Depth = proj::NormalizeDepth(proj::z_distance(*where));
	auto frameTime = loader::query_float_attribute(groupIndex, 0, 903);
	if (frameTime && frameTime[0] > 0.0f)
		FrameTime = frameTime[0];
	auto autoStart = loader::query_float_attribute(groupIndex, 0, 905);
	AutoStart = autoStart && autoStart[0] > 0.0f;

	RenderSprite = new render_sprite(VisualTypes::Ball, nullptr, nullptr, X, Y, nullptr);
	AddFrames(groupIndex);
	if (AutoStart)
		Start();
}

TFT2Animation::~TFT2Animation()
{
	if (Timer)
		timer::kill(Timer);
	Timer = 0;
}

void TFT2Animation::AddFrames(int groupIndex)
{
	auto states = loader::query_visual_states(groupIndex);
	if (states < 1)
		states = 1;
	for (int state = 0; state < states; state++)
	{
		visualStruct visual{};
		loader::query_visual(groupIndex, state, &visual);
		if (visual.Bitmap.Bmp)
			Frames.push_back(visual.Bitmap.Bmp);
	}
}

void TFT2Animation::AddFrameObject(TPinballTable* table, int groupIndex)
{
	auto base = FrameBaseName(loader::query_name(groupIndex));
	for (auto component : table->ComponentList)
	{
		auto animation = dynamic_cast<TFT2Animation*>(component);
		if (animation && animation->BaseName == base)
		{
			animation->AddFrames(groupIndex);
			return;
		}
	}
	new TFT2Animation(table, groupIndex);
}

void TFT2Animation::Start()
{
	if (Running || Frames.empty())
		return;
	Running = true;
	Current = 0;
	ShowFrame();
	Timer = timer::set(FrameTime, this, Tick);
}

void TFT2Animation::Stop()
{
	Running = false;
	if (Timer)
		timer::kill(Timer);
	Timer = 0;
	RenderSprite->set(nullptr, nullptr, X, Y);
}

void TFT2Animation::ShowFrame()
{
	auto bmp = Frames[Current % Frames.size()];
	RenderSprite->set(bmp, nullptr, X, Y);
	RenderSprite->Depth = Depth;
}

void TFT2Animation::Tick(int, void* caller)
{
	auto animation = static_cast<TFT2Animation*>(caller);
	animation->Timer = 0;
	if (!animation->Running)
		return;
	animation->Current = (animation->Current + 1) % animation->Frames.size();
	animation->ShowFrame();
	animation->Timer = timer::set(animation->FrameTime, animation, Tick);
}

int TFT2Animation::Message(MessageCode code, float value)
{
	// Timers are cleared on a new game; keep auto-started animations running
	if ((code == MessageCode::Reset || code == MessageCode::NewGame) && AutoStart)
	{
		Running = false;
		Timer = 0;
		Start();
	}
	return 0;
}
