#pragma once
// In-app updater: checks the latest GitHub release of Full Tilt Native and, only when the user agrees,
// downloads it, replaces the program files (never tables/ or "Put game files here") and restarts the launcher.
#include <filesystem>
#include <mutex>
#include <string>

namespace updater
{
	enum class State { Idle, Checking, UpToDate, Available, Downloading, Installing, Done, Failed, Unsupported };

	struct Status
	{
		State state = State::Idle;
		std::string latestVersion;  // e.g. v0.2.0
		std::string downloadUrl;    // release zip
		std::string releaseNotes;
		std::string message;        // human readable status / error
	};

	extern const char* CurrentVersion;

	Status Get();
	void CheckAsync();                                   // look for a newer release (background)
	void InstallAsync(const std::filesystem::path& appDir); // download + replace files (background)
	void CleanupOldFiles(const std::filesystem::path& appDir); // remove *.old left by a previous update
	bool Restart(const std::filesystem::path& appDir);   // start the new launcher
	// Close Full Tilt programs (launcher, engine) running from appDir so their files can be replaced; returns how many
	int CloseRunningPrograms(const std::filesystem::path& appDir, const std::string& except);
	// Start a program from appDir with arguments; returns true if started
	bool StartProgram(const std::filesystem::path& appDir, const std::string& exeName, const std::string& args);
	// Run a command line without a window and wait for it (Windows); returns true on exit code 0
	bool RunHidden(const std::string& commandLine, const std::string& workDir);
}
