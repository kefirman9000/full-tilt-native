#pragma once
#include <vector>
#include "TPinballComponent.h"

class TLight;

// Full Tilt 1 light sweep (type 1038, Dragon's Keep's light_sweeps; engine 10:3324 / 10:3979): the table's lights in
// a 15 x 25 grid over the playfield; Ft1LightSweepStart (FT1 27) with sweep number n reads that sweep's attributes
// (1600 centre, 1601 outward, 1602 step time, 1603 passes, 1604 light time) and draws rings around the centre, one
// ring per step, each light of a ring lit for a while (FT1 24); after the last pass the rules hear
// ControlTimerExpired (FT1 70).
class TLightSweeps :
	public TPinballComponent
{
public:
	TLightSweeps(TPinballTable* table, int groupIndex);
	int Message(MessageCode code, float value) override;

private:
	void Start(int sweep);
	void Step();                  // 10:3880
	void Ring(int x, int y, int r); // 10:31be
	void Cell(int x, int y);      // 10:30d8
	void Stop();                  // 10:384d
	static void TimerExpired(int timerId, void* caller);

	static constexpr int Cols = 15, Rows = 25;
	std::vector<TLight*> Cells[Cols * Rows];
	float XMin = 0, YMin = 0, ScaleX = 1, ScaleY = 1; // +0x54/+0x58, +0x48/+0x4c
	float StepTime = 0.1f, LightTime = 0.1f;          // +0x5c, +0x60
	int CentreX = 0, CentreY = 0;                     // +0x64, +0x66
	int Outward = 0, Passes = 1;                      // +0x68, +0x6a
	int Radius = 0, MaxRadius = 0;                    // +0x6c, +0x6e
	int Timer = 0;                                    // +0x70
};
