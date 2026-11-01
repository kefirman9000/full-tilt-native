#pragma once
// Full Tilt Native music helpers: CD-audio replacement files and each table's own MIDI track names.
#include <string>
#include <vector>

namespace ftmusic
{
	// Audio files (mp3, ogg, wav, flac, opus) in a folder, sorted by name. Empty if the folder is missing.
	std::vector<std::string> ListAudioFiles(const std::string& folder);
	// Prefix of the table's MIDI tracks in its sound folder, e.g. "TABE" for TABE1.MDS / tabe1.mid. Empty if none.
	std::string FindMidiPrefix(const std::string& soundFolder);
}
