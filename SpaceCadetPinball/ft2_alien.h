#pragma once
#include <string>
#include <vector>

class TPinballComponent;
enum class MessageCode;

// Alien Daze's own rules, recovered from Alien.exe (alien\src\control.cpp; rule table at 0x442dd8, notes in
// ft2/alien_rules.md): five missions, spacecraft landing and ball lock, tractor beam, stun magnet, reserve power,
// score multiplier, bonus hold, artifact trading and the end-of-ball bonus.
// ft2_control calls these for ALIEN.DAT instead of its generic rules.
class ft2_alien
{
public:
	static bool Active();
	// A part was hit (ControlCollision): scores it and runs its rule. Returns false for parts it leaves alone.
	static bool Collision(TPinballComponent* caller);
	// Any other message from a part (flipper presses, ramp passes...): true when used
	static bool Message(MessageCode code, TPinballComponent* caller);
	static void Launch(); // a ball left the plunger
	static void NewGame();
	static void PlayerChanged();
	static void EndOfBall(); // before the next ball: bonus count
	static void Frame();     // every frame: the stun magnet's pull
	static std::vector<std::string> Status();
};
