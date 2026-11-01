// Full Tilt Native logging, see ft_log.h
#include "ft_log.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace ftlog
{
	static std::string& Path()
	{
		static std::string path;
		return path;
	}

	const std::string& FilePath()
	{
		return Path();
	}

	void Line(const char* text)
	{
		time_t now = time(nullptr);
		char stamp[32];
		strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
		fprintf(stderr, "[%s] %s\n", stamp, text);
		fflush(stderr);
	}

	void Line(const std::string& text)
	{
		Line(text.c_str());
	}

#ifdef _WIN32
	static LONG WINAPI OnCrash(EXCEPTION_POINTERS* info)
	{
		auto code = info->ExceptionRecord->ExceptionCode;
		auto address = info->ExceptionRecord->ExceptionAddress;
		auto base = reinterpret_cast<char*>(GetModuleHandleA(nullptr));
		char text[256];
		snprintf(text, sizeof text, "CRASH: exception 0x%08lX at %p (program offset 0x%llX). Please send this log file.",
		         static_cast<unsigned long>(code), address,
		         static_cast<unsigned long long>(reinterpret_cast<char*>(address) - base));
		Line(text);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	static FILE* Reopen(const std::string& path, FILE* stream)
	{
		int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
		std::wstring wide(size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wide[0], size);
		return _wfreopen(wide.c_str(), L"a", stream);
	}
#else
	static void OnCrash(int sig)
	{
		char text[128];
		snprintf(text, sizeof text, "CRASH: signal %d. Please send this log file.", sig);
		Line(text);
		_exit(128 + sig);
	}

	static FILE* Reopen(const std::string& path, FILE* stream)
	{
		return freopen(path.c_str(), "a", stream);
	}
#endif

	static void OnExit()
	{
		Line("exited");
	}

	void Init(const char* logName, const char* appName)
	{
		char* base = SDL_GetBasePath();
		std::string dir = base ? base : "";
		SDL_free(base);
		Path() = dir + logName;

		// Keep the log from growing forever: past 2 MB the old one is kept as <name>.old
		if (auto file = fopen(Path().c_str(), "rb"))
		{
			fseek(file, 0, SEEK_END);
			long size = ftell(file);
			fclose(file);
			if (size > 2 * 1024 * 1024)
			{
				std::string old = Path() + ".old";
				remove(old.c_str());
				rename(Path().c_str(), old.c_str());
			}
		}

		if (Reopen(Path(), stdout))
			setvbuf(stdout, nullptr, _IONBF, 0);
		if (Reopen(Path(), stderr))
			setvbuf(stderr, nullptr, _IONBF, 0);

		fprintf(stderr, "\n==================== %s ====================\n", appName);
		Line("started");
		SDL_LogSetOutputFunction([](void*, int, SDL_LogPriority priority, const char* message)
		{
			fprintf(stderr, "SDL%s: %s\n", priority >= SDL_LOG_PRIORITY_ERROR ? " ERROR" : "", message);
		}, nullptr);

#ifdef _WIN32
		SetUnhandledExceptionFilter(OnCrash);
#else
		for (int sig : {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS})
			signal(sig, OnCrash);
#endif
		atexit(OnExit);
	}

	static bool FileExists(const std::string& path)
	{
		if (auto file = fopen(path.c_str(), "rb"))
		{
			fclose(file);
			return true;
		}
		return false;
	}

	bool StartLauncherInstead(const char* commandLine)
	{
		std::string args = commandLine ? commandLine : "";
		if (args.find("--table") != std::string::npos)
			return false;
		char* base = SDL_GetBasePath();
		std::string dir = base ? base : "";
		SDL_free(base);
		for (auto dat : {"CADET.DAT", "cadet.dat", "PINBALL.DAT", "pinball.dat", "DEMO.DAT", "demo.dat"})
			if (FileExists(dat) || FileExists(dir + dat))
				return false;
		if (!StartLauncher())
			return false;
		Line("No table given: opened the table library (FullTiltPinball.exe) instead");
		return true;
	}

	bool StartLauncher()
	{
		char* base = SDL_GetBasePath();
		std::string dir = base ? base : "";
		SDL_free(base);
#ifdef _WIN32
		std::string launcher = dir + "FullTiltPinball.exe";
		if (!FileExists(launcher))
			return false;
		int size = MultiByteToWideChar(CP_UTF8, 0, launcher.c_str(), -1, nullptr, 0);
		std::wstring wide(size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, launcher.c_str(), -1, &wide[0], size);
		wide.resize(size ? size - 1 : 0);
		std::wstring cmd = L"\"" + wide + L"\"";
		int dsize = MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0);
		std::wstring wdir(dsize, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, &wdir[0], dsize);
		STARTUPINFOW si{};
		si.cb = sizeof si;
		PROCESS_INFORMATION pi{};
		if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, wdir.c_str(), &si, &pi))
			return false;
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
#else
		return false;
#endif
	}
}
