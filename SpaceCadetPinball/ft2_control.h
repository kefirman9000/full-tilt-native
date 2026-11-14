#pragma once

enum class MessageCode;
class TPinballComponent;
class TBall;
#include <string>
#include <vector>

// Generic table rules for Full Tilt! 2 tables (Alien, Hero, Mad) until each table's real rules are recovered from its
// original exe. Gives every table a playable game: points for hits, captured balls are kicked back out, the drain serves
// the next ball, and the game ends after the last ball.
class ft2_control
{
public:
	static void handler(MessageCode code, TPinballComponent* caller);
	// Table-wide events (new game, game over, next player)
	static void table_handler(MessageCode code);
	// Every frame: a ball that has left the table's area is given back at the plunger (an engine fault, not a lost ball)
	static void check_escaped_balls();
	// A ball was served to the plunger and has not been launched yet
	static bool AwaitingLaunch();
	// Where the ball rests in the shooter lane (set when the plunger is pulled with a ball waiting there)
	static bool PlungerRest(float& x, float& y);
	// The ball whose collision is being handled (set by the physics loop just before it), for rules that look at
	// the ball itself, such as Captain Hero's skill shot (only a slow ball counts)
	static TBall* CollidingBall;
	// Short lines for the side panel about what the rules are doing now (bonus multiplier, lit pockets, ...)
	static std::vector<std::string> StatusLines();
	// Practice mode was on at some point this game: its scores are not high scores or statistics
	static bool PracticeGame;
};
