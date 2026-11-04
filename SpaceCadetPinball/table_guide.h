#pragma once

class TPinballTable;

// Help > Table Guide: how the table in play scores (from the rules recovered so far) and the player's statistics
// for it (games, best and average game, time played), kept per table in the settings file.
class table_guide
{
public:
	static bool ShowWindow;
	static void Render();
	// Called by the table: a game starts, a game ends (records every player's score)
	static void GameStarted();
	static void GameOver(TPinballTable* table);
};
