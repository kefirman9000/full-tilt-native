#pragma once
#include <string>
#include <vector>

class TPinballComponent;
enum class MessageCode;

// Mad Scientist's own rules, recovered from mad.exe (rule table of 53 entries; notes in ft2/mad_rules.md): the five
// experiments started at Igor, the flask of elements mixed into potions, monster parts, brains, the teleporter,
// clone ball, plasma field, lightning rod, combos, skill shot and the end-of-ball bonus.
// ft2_control calls these for MAD.DAT instead of its generic rules.
class ft2_mad
{
public:
	static bool Active();
	// A part was hit (ControlCollision): scores it and runs its rule. Returns false for parts it leaves alone.
	static bool Collision(TPinballComponent* caller);
	// Any other message from a part (flipper presses): true when used
	static bool Message(MessageCode code, TPinballComponent* caller);
	static void Launch(); // a ball left the plunger
	static void NewGame();
	static void PlayerChanged();
	static void EndOfBall(); // before the next ball: bonus count
	// The last ball drained: true when the shoot again light or the ball recapture gives it back
	static bool SaveBall();
	static std::vector<std::string> Status();
};
