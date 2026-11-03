#pragma once
#include <string>

// Table messages of the Full Tilt 1 tables (Dragon's Keep, Skulduggery). Their rules show texts from string resources
// inside the table's own program (DRAGON.EXE / PIRATES.EXE, a 16-bit Windows NE file). They are read from the
// player's copy in the table folder when the table loads; no game text is stored in this project.
namespace ft1_strings
{
	// Load the RT_STRING resources of the NE executable at exePath; false if it is missing or not an NE file
	bool Load(const std::string& exePath);
	// String with resource id, or "" if there is none
	const char* Get(int id);
	bool Loaded();
}
