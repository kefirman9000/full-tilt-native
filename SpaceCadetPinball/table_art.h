#pragma once
struct SDL_Surface;

// Pictures of the table for the side panel and the "table art" backdrop, from the player's own files (nothing of the
// original game is stored in this project): panel.bmp if the player put one in the table folder, else the title
// screen inside the table's own program (Full Tilt 2's Alien.exe / hero.exe / mad.exe keep a 640x480 SPLASH_BITMAP,
// and the launcher copies the program next to the table's data), else the launcher's preview.bmp (the table icon).
class table_art
{
public:
	// A new surface (the caller frees it) or nullptr
	static SDL_Surface* Load();
	// The title screen bitmap resource of a 32-bit Windows program, or nullptr
	static SDL_Surface* LoadExeBitmap(const std::string& exePath, const char* resourceName);
};
