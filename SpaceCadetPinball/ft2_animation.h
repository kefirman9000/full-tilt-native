#pragma once
#include "TPinballComponent.h"
#include <string>
#include <vector>

struct gdrv_bitmap8;

// Full Tilt animations the Space Cadet decomp never had:
//  type 1044: one object with several frames (Alien Daze's sockets and beam-outs)
//  type 1045: one object per frame, frames share a name (Mad Scientist's bubbling_01..25, flask_mixing_01..16...)
// Drawn like the ball: a picture at a screen position with a depth, so ramps and toys in front still cover it.
// Only animations flagged to run on their own (attribute 905, e.g. Mad's bubbling plunger tube) play for now;
// the rest are started by table rules that don't exist yet.
class TFT2Animation : public TPinballComponent
{
public:
	TFT2Animation(TPinballTable* table, int groupIndex);
	~TFT2Animation() override;
	// Type 1045: add this group as the next frame of the animation it belongs to (created on its first frame)
	static void AddFrameObject(TPinballTable* table, int groupIndex);
	int Message(MessageCode code, float value) override;
	void Start();
	void Stop();
	// Mad Scientist's plunger tube bubbles while the plunger is pulled back, not all the time
	bool PlungerLinked = false;

private:
	void AddFrames(int groupIndex);
	void ShowFrame();
	static void Tick(int timerId, void* caller);

	std::vector<gdrv_bitmap8*> Frames;
	std::string BaseName;
	int X = 0, Y = 0;
	uint16_t Depth = 0;
	float FrameTime = 0.08f;
	size_t Current = 0;
	int Timer = 0;
	bool Running = false;
	bool AutoStart = false;
};
