#pragma once

enum class MessageCode;
class TPinballComponent;

// Generic table rules for Full Tilt! 2 tables (Alien, Hero, Mad) until each table's real rules are recovered from its
// original exe. Gives every table a playable game: points for hits, captured balls are kicked back out, the drain serves
// the next ball, and the game ends after the last ball.
class ft2_control
{
public:
	static void handler(MessageCode code, TPinballComponent* caller);
};
