#pragma once

// Game controller rumble (Options > Controller Rumble): a short pulse on every connected controller that can rumble.
// low/high are the strengths of the two motors (0..1).
class rumble
{
public:
	static void Pulse(float low, float high, int ms);
};
