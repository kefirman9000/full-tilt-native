// Full Tilt Native music helpers, see ft_music.h
#include "ft_music.h"

#include <algorithm>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace ftmusic
{
	static std::string Lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	static std::vector<std::string> ListFiles(const std::string& folder)
	{
		std::vector<std::string> names;
#ifdef _WIN32
		std::string pattern = folder + "\\*";
		int size = MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, nullptr, 0);
		std::wstring wide(size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, &wide[0], size);
		WIN32_FIND_DATAW data;
		HANDLE find = FindFirstFileW(wide.c_str(), &data);
		if (find == INVALID_HANDLE_VALUE)
			return names;
		do
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			int len = WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, nullptr, 0, nullptr, nullptr);
			std::string name(len, '\0');
			WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, &name[0], len, nullptr, nullptr);
			name.resize(len ? len - 1 : 0);
			names.push_back(name);
		}
		while (FindNextFileW(find, &data));
		FindClose(find);
#else
		if (DIR* dir = opendir(folder.c_str()))
		{
			while (dirent* entry = readdir(dir))
				if (entry->d_name[0] != '.')
					names.push_back(entry->d_name);
			closedir(dir);
		}
#endif
		std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return Lower(a) < Lower(b); });
		return names;
	}

	std::vector<std::string> ListAudioFiles(const std::string& folder)
	{
		std::vector<std::string> files;
		for (auto& name : ListFiles(folder))
		{
			auto dot = name.rfind('.');
			if (dot == std::string::npos)
				continue;
			auto ext = Lower(name.substr(dot));
			if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".flac" || ext == ".opus")
				files.push_back(folder + (folder.empty() ? "" : "/") + name);
		}
		return files;
	}

	std::string FindMidiPrefix(const std::string& soundFolder)
	{
		// Table music is TAB<letter>1..8 (.MID in the 16-bit version, .MDS in the 32-bit one)
		for (auto& name : ListFiles(soundFolder))
		{
			auto lower = Lower(name);
			if (lower.size() == 9 && lower.compare(0, 3, "tab") == 0 && lower[4] == '1' &&
				(lower.compare(5, 4, ".mid") == 0 || lower.compare(5, 4, ".mds") == 0))
				return "TAB" + std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(lower[3]))));
		}
		return "";
	}
}
