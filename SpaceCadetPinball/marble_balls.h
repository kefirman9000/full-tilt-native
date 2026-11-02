#pragma once
// Marble Drop marbles as ball styles (Options > Ball > Marble Drop ...)
//
// Marble Drop (Maxis South, 1997) runs on the same engine as Full Tilt: its COREA.DAT / COREB.DAT are PARTOUT(4.0)
// resource files, and the "marbles" group holds one 8-bit sprite per marble colour. The launcher stores the user's
// Marble Drop install in tables/_MarbleDrop; this module reads the marbles from there (nothing is shipped) and
// scales each one to the sizes of the table's own ball sprites, so depth, physics and size stay the table's.

#include <string>
#include <vector>

struct gdrv_bitmap8;

namespace marbles
{
	// Style names ("marble_red", ...), empty when no Marble Drop data was found
	const std::vector<std::string>& Styles();
	bool IsMarbleStyle(const std::string& style);
	// Menu label for a marble style ("Marble Drop: Red")
	std::string Label(const std::string& style);
	// The marble scaled to each of the standard ball frames (same count and sizes). False if it can't be made.
	bool Build(const std::string& style, const std::vector<gdrv_bitmap8*>& standardFrames,
	           std::vector<gdrv_bitmap8*>& out);
}
