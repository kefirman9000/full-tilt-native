#include "pch.h"
#include "TLightSweeps.h"

#include "control.h"
#include "loader.h"
#include "timer.h"
#include "TLight.h"
#include "TLightGroup.h"
#include "TPinballTable.h"
#include "TTableLayer.h"

namespace
{
	int GridIndex(float value, float origin, float scale, int count) // floor, clamped to the grid
	{
		auto v = std::floor((value - origin) * scale);
		if (v > count - 1)
			v = static_cast<float>(count - 1);
		if (v <= 0)
			v = 0;
		return static_cast<int>(v);
	}
}

TLightSweeps::TLightSweeps(TPinballTable* table, int groupIndex) : TPinballComponent(table, groupIndex, false)
{
	TTableLayer* layer = nullptr;
	for (auto comp : table->ComponentList)
		if ((layer = dynamic_cast<TTableLayer*>(comp)) != nullptr)
			break;
	if (layer)
	{
		XMin = layer->XMin;
		YMin = layer->YMin;
		ScaleX = Cols / (layer->XMax - layer->XMin);
		ScaleY = Rows / (layer->YMax - layer->YMin);
	}
	// every light of the table (table->LightGroup, the ones loaded so far) goes into the cell of its position (501)
	if (table->LightGroup)
		for (int i = static_cast<int>(table->LightGroup->List.size()) - 1; i >= 0; i--)
		{
			auto light = table->LightGroup->List[i];
			auto pos = loader::query_float_attribute(light->GroupIndex, 0, 501);
			if (!pos)
				continue;
			auto row = GridIndex(pos[1], YMin, ScaleY, Rows);
			auto col = GridIndex(pos[0], XMin, ScaleX, Cols);
			Cells[row * Cols + col].push_back(light);
		}
}

int TLightSweeps::Message(MessageCode code, float value)
{
	switch (code)
	{
	case MessageCode::Reset:
	case MessageCode::SetTiltLock:
	case MessageCode::GameOver:
		Stop();
		break;
	case MessageCode::Ft1LightSweepStart:
		if (GroupIndex)
			Start(static_cast<int>(std::lround(value)));
		break;
	default:
		break;
	}
	return 0;
}

void TLightSweeps::Start(int sweep)
{
	auto attr = [&](int id) { return loader::query_float_attribute(GroupIndex, sweep, id); };
	auto stepTime = attr(1602);
	StepTime = stepTime ? stepTime[0] : 0.1f;
	auto lightTime = attr(1604);
	LightTime = lightTime ? lightTime[0] : StepTime;
	if (auto centre = attr(1600))
	{
		CentreX = GridIndex(centre[0], XMin, ScaleX, Cols);
		CentreY = GridIndex(centre[1], YMin, ScaleY, Rows);
	}
	else
		CentreX = CentreY = 0;
	auto outward = attr(1601);
	Outward = outward ? static_cast<int>(outward[0]) : 0;
	auto passes = attr(1603);
	Passes = passes ? static_cast<int>(passes[0]) : 1;
	// the largest distance to a corner of the grid
	auto sq = [](int a, int b) { return a * a + b * b; };
	int far = std::max(std::max(sq(Cols - 1 - CentreX, Rows - 1 - CentreY), sq(CentreX, CentreY)),
	                   std::max(sq(Cols - 1 - CentreX, CentreY), sq(CentreX, Rows - 1 - CentreY)));
	MaxRadius = Radius = static_cast<int>(std::sqrt(static_cast<double>(far)));
	if (Outward)
		Radius = 0;
	Step();
}

void TLightSweeps::Step()
{
	bool done = false;
	if (!Outward)
		done = --Radius < 0;
	else
		done = ++Radius > MaxRadius;
	if (done && --Passes > 0)
	{
		done = false;
		Radius = Outward ? 0 : MaxRadius;
	}
	if (done)
	{
		control::handler(MessageCode::ControlTimerExpired, this); // FT1 70
		return;
	}
	Ring(CentreX, CentreY, Radius);
	Timer = timer::set(StepTime, this, TimerExpired);
}

void TLightSweeps::Ring(int cx, int cy, int r)
{
	int x = 0, y = r;
	uint32_t d = static_cast<uint32_t>(r * r), e = static_cast<uint32_t>(r * r - r);
	do
	{
		Cell(cx + x, cy - y);
		Cell(cx - x, cy - y);
		Cell(cx + x, cy + y);
		Cell(cx - x, cy + y);
		Cell(cx + y, cy - x);
		Cell(cx - y, cy - x);
		Cell(cx + y, cy + x);
		Cell(cx - y, cy + x);
		x++;
		d -= static_cast<uint32_t>(2 * x - 1);
		if (!(d > e))
		{
			y--;
			e -= static_cast<uint32_t>(2 * y);
		}
	}
	while (x <= y);
}

void TLightSweeps::Cell(int x, int y)
{
	if (x < 0 || x >= Cols || y < 0 || y >= Rows)
		return;
	auto& cell = Cells[y * Cols + x];
	for (auto it = cell.rbegin(); it != cell.rend(); ++it)
		(*it)->Message(MessageCode::TLightFtTmpOverrideOn, LightTime); // FT1 24
}

void TLightSweeps::Stop()
{
	if (Timer)
		timer::kill(Timer);
	Timer = 0;
}

void TLightSweeps::TimerExpired(int timerId, void* caller)
{
	auto sweeps = static_cast<TLightSweeps*>(caller);
	sweeps->Timer = 0;
	sweeps->Step();
}
