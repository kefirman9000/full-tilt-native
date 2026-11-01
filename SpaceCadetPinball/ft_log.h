#pragma once
// Full Tilt Native logging: sends everything the program prints (stdout, stderr, SDL messages) to a log file in the
// program's folder, with timestamps, and records crashes (exception code + location) before exiting.
// Launcher: FullTiltPinball.log, engine: FullTiltEngine.log. Implementation in ft_log.cpp.
#include <string>

namespace ftlog
{
	// Call first thing in main. logName: file name created in the program's folder.
	void Init(const char* logName, const char* appName);
	// Timestamped line in the log
	void Line(const char* text);
	void Line(const std::string& text);
	const std::string& FilePath();

	// The engine was started without a table (e.g. FullTiltEngine.exe double-clicked) and no Space Cadet data is next
	// to it: open the table library launcher instead. Returns true if the launcher was started.
	bool StartLauncherInstead(const char* commandLine);
}
