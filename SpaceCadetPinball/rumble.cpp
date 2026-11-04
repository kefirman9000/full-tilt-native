#include "pch.h"
#include "rumble.h"

#include "options.h"

void rumble::Pulse(float low, float high, int ms)
{
	if (!options::Options.Rumble)
		return;
	auto motor = [](float strength)
	{
		return static_cast<Uint16>(std::max(0.0f, std::min(1.0f, strength)) * 0xFFFF);
	};
	for (int index = 0; index < SDL_NumJoysticks(); index++)
	{
		if (!SDL_IsGameController(index))
			continue;
		// Controllers are opened when they connect (winmain); only those get a pulse
		if (auto controller = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(index)))
			SDL_GameControllerRumble(controller, motor(low), motor(high), static_cast<Uint32>(ms));
	}
}
