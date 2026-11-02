#include "updater.h"
#include "../SpaceCadetPinball/ft_log.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <tlhelp32.h>
#endif

#ifndef FTN_VERSION
#define FTN_VERSION "dev"
#endif

namespace fs = std::filesystem;

namespace updater
{
	const char* CurrentVersion = FTN_VERSION;
	static const char* Repo = "kefirman9000/full-tilt-native";
	static const char* AssetSuffix = "-windows-x64.zip";

	static std::mutex Lock;
	static Status Current;

	static void Set(State state, const std::string& message)
	{
		std::lock_guard<std::mutex> guard(Lock);
		Current.state = state;
		Current.message = message;
		ftlog::Line("Updater: " + message);
	}

	Status Get()
	{
		std::lock_guard<std::mutex> guard(Lock);
		return Current;
	}

	// "v1.2.3" -> comparable number; "dev" and anything unparsable -> 0 (always offered an update)
	static long long VersionNumber(const std::string& version)
	{
		long long parts[3]{};
		int index = 0;
		std::string digits;
		for (char c : version + ".")
		{
			if (c >= '0' && c <= '9')
				digits += c;
			else if (c == '.' || c == '-')
			{
				if (!digits.empty() && index < 3)
					parts[index++] = std::atoll(digits.c_str());
				digits.clear();
				if (c == '-')
					break;
			}
		}
		return parts[0] * 1000000 + parts[1] * 1000 + parts[2];
	}

	// Minimal JSON string reader for the few fields we need from the GitHub API
	static std::string JsonString(const std::string& json, const std::string& key, size_t from = 0, size_t* end = nullptr)
	{
		auto pos = json.find("\"" + key + "\"", from);
		if (pos == std::string::npos)
			return "";
		pos = json.find(':', pos);
		pos = json.find('"', pos);
		if (pos == std::string::npos)
			return "";
		std::string value;
		for (size_t i = pos + 1; i < json.size(); i++)
		{
			char c = json[i];
			if (c == '\\' && i + 1 < json.size())
			{
				char n = json[++i];
				value += n == 'n' ? '\n' : n == 'r' ? '\r' : n == 't' ? '\t' : n;
				continue;
			}
			if (c == '"')
			{
				if (end)
					*end = i;
				break;
			}
			value += c;
		}
		return value;
	}

#ifdef _WIN32
	static std::wstring Wide(const std::string& s)
	{
		int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(size, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], size);
		w.resize(size ? size - 1 : 0);
		return w;
	}

	// HTTPS GET with Windows' built-in WinHTTP (follows GitHub's redirect to its download servers)
	static bool HttpGet(const std::string& url, std::string& body, std::string& error)
	{
		auto wurl = Wide(url);
		URL_COMPONENTS parts{};
		parts.dwStructSize = sizeof parts;
		wchar_t host[256], path[2048];
		parts.lpszHostName = host;
		parts.dwHostNameLength = 256;
		parts.lpszUrlPath = path;
		parts.dwUrlPathLength = 2048;
		if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
		{
			error = "bad URL";
			return false;
		}
		std::wstring fullPath(parts.lpszUrlPath, parts.dwUrlPathLength);
		std::wstring hostName(parts.lpszHostName, parts.dwHostNameLength);

		HINTERNET session = WinHttpOpen(L"FullTiltNative-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
			session = WinHttpOpen(L"FullTiltNative-Updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		HINTERNET connect = session ? WinHttpConnect(session, hostName.c_str(), parts.nPort, 0) : nullptr;
		HINTERNET request = connect
			                    ? WinHttpOpenRequest(connect, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER,
			                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
			                                         parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
			                    : nullptr;
		bool ok = false;
		if (request &&
			WinHttpSendRequest(request, L"Accept: application/vnd.github+json, application/octet-stream\r\n", -1L,
			                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			WinHttpReceiveResponse(request, nullptr))
		{
			DWORD statusCode = 0, size = sizeof statusCode;
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			                    WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
			DWORD available = 0;
			while (WinHttpQueryDataAvailable(request, &available) && available)
			{
				std::vector<char> buffer(available);
				DWORD read = 0;
				if (!WinHttpReadData(request, buffer.data(), available, &read) || !read)
					break;
				body.append(buffer.data(), read);
			}
			ok = statusCode == 200;
			if (!ok)
				error = "server answered " + std::to_string(statusCode) +
					(statusCode == 404 ? " (no public release found - is the GitHub repository public?)" : "");
		}
		else
			error = "no connection (error " + std::to_string(GetLastError()) + ")";
		if (request) WinHttpCloseHandle(request);
		if (connect) WinHttpCloseHandle(connect);
		if (session) WinHttpCloseHandle(session);
		return ok;
	}

	static bool RunAndWait(const std::string& commandLine, const std::string& workDir)
	{
		STARTUPINFOW si{};
		si.cb = sizeof si;
		PROCESS_INFORMATION pi{};
		auto cmd = Wide(commandLine);
		auto dir = Wide(workDir);
		if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
		                    dir.empty() ? nullptr : dir.c_str(), &si, &pi))
			return false;
		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD exitCode = 1;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return exitCode == 0;
	}
#endif

	void CheckAsync()
	{
#ifndef _WIN32
		Set(State::Unsupported, "updates are Windows only");
		return;
#else
		Set(State::Checking, "checking GitHub for a newer version...");
		std::thread([]
		{
			std::string body, error;
			if (!HttpGet(std::string("https://api.github.com/repos/") + Repo + "/releases/latest", body, error))
			{
				Set(State::Failed, "could not check for updates: " + error);
				return;
			}
			auto tag = JsonString(body, "tag_name");
			std::string url;
			for (size_t pos = 0;;)
			{
				size_t end = 0;
				auto candidate = JsonString(body, "browser_download_url", pos, &end);
				if (candidate.empty())
					break;
				if (candidate.size() > strlen(AssetSuffix) &&
					candidate.compare(candidate.size() - strlen(AssetSuffix), std::string::npos, AssetSuffix) == 0)
					url = candidate;
				pos = end + 1;
			}
			{
				std::lock_guard<std::mutex> guard(Lock);
				Current.latestVersion = tag;
				Current.downloadUrl = url;
				Current.releaseNotes = JsonString(body, "body");
			}
			if (tag.empty() || url.empty())
				Set(State::Failed, "latest release has no Windows download");
			else if (VersionNumber(tag) > VersionNumber(CurrentVersion))
				Set(State::Available, "version " + tag + " is available (you have " + CurrentVersion + ")");
			else
				Set(State::UpToDate, std::string("up to date (") + CurrentVersion + ")");
		}).detach();
#endif
	}

	void InstallAsync(const fs::path& appDir)
	{
#ifdef _WIN32
		auto url = Get().downloadUrl;
		Set(State::Downloading, "downloading " + Get().latestVersion + "...");
		std::thread([url, appDir]
		{
			std::error_code ec;
			auto work = fs::temp_directory_path(ec) / "FullTiltNativeUpdate";
			fs::remove_all(work, ec);
			fs::create_directories(work, ec);
			auto zip = work / "update.zip";

			std::string data, error;
			if (!HttpGet(url, data, error))
			{
				Set(State::Failed, "download failed: " + error);
				return;
			}
			{
				std::ofstream out(zip, std::ios::binary);
				out.write(data.data(), static_cast<std::streamsize>(data.size()));
			}

			Set(State::Installing, "installing...");
			// tar.exe ships with Windows 10 and 11 and unpacks zip files
			auto tar = std::string(getenv("SystemRoot") ? getenv("SystemRoot") : "C:\\Windows") + "\\System32\\tar.exe";
			if (!RunAndWait("\"" + tar + "\" -xf \"" + zip.string() + "\"", work.string()))
			{
				Set(State::Failed, "could not unpack the update");
				return;
			}
			// The release zip holds one top folder (FullTiltNative); find the folder that has the launcher in it
			fs::path source;
			for (auto& entry : fs::recursive_directory_iterator(work, ec))
				if (entry.path().filename() == "FullTiltPinball.exe")
				{
					source = entry.path().parent_path();
					break;
				}
			if (source.empty())
			{
				Set(State::Failed, "update package is missing FullTiltPinball.exe");
				return;
			}

			// Replace program files only (top-level files). A running exe can't be overwritten but can be renamed,
			// so each existing file is renamed to *.old first; the next start deletes those.
			for (auto& entry : fs::directory_iterator(source, ec))
			{
				if (!entry.is_regular_file(ec))
					continue;
				auto target = appDir / entry.path().filename();
				auto old = target;
				old += ".old";
				fs::remove(old, ec);
				if (fs::exists(target, ec))
				{
					fs::rename(target, old, ec);
					if (ec)
					{
						Set(State::Failed, "could not replace " + target.filename().string() +
						    " - close the game and try again");
						return;
					}
				}
				fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
				if (ec)
				{
					fs::rename(old, target, ec); // put the old one back
					Set(State::Failed, "could not install " + target.filename().string());
					return;
				}
			}
			fs::remove_all(work, ec);
			Set(State::Done, "updated to " + Get().latestVersion + " - restarting");
		}).detach();
#else
		(void)appDir;
#endif
	}

	void CleanupOldFiles(const fs::path& appDir)
	{
		std::error_code ec;
		for (auto& entry : fs::directory_iterator(appDir, ec))
			if (entry.is_regular_file(ec) && entry.path().extension() == ".old" &&
				entry.path().filename().string().find(".log") == std::string::npos)
				fs::remove(entry.path(), ec);
	}

	bool Restart(const fs::path& appDir)
	{
#ifdef _WIN32
		STARTUPINFOW si{};
		si.cb = sizeof si;
		PROCESS_INFORMATION pi{};
		auto exe = (appDir / "FullTiltPinball.exe").wstring();
		std::wstring cmd = L"\"" + exe + L"\"";
		if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, appDir.wstring().c_str(), &si, &pi))
			return false;
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
#else
		(void)appDir;
		return false;
#endif
	}

	int CloseRunningPrograms(const fs::path& appDir, const std::string& except)
	{
		int closed = 0;
#ifdef _WIN32
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
			return 0;
		PROCESSENTRY32W entry{};
		entry.dwSize = sizeof entry;
		std::error_code ec;
		auto dir = fs::weakly_canonical(appDir, ec);
		for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry))
		{
			std::wstring name = entry.szExeFile;
			if (_wcsicmp(name.c_str(), L"FullTiltPinball.exe") != 0 && _wcsicmp(name.c_str(), L"FullTiltEngine.exe") != 0)
				continue;
			if (!except.empty() && _wcsicmp(name.c_str(), Wide(except).c_str()) == 0)
				continue;
			HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
			                             entry.th32ProcessID);
			if (!process)
				continue;
			wchar_t path[MAX_PATH * 2];
			DWORD size = MAX_PATH * 2;
			if (QueryFullProcessImageNameW(process, 0, path, &size) &&
				fs::weakly_canonical(fs::path(path).parent_path(), ec) == dir)
			{
				ftlog::Line("Updater: closing " + fs::path(path).filename().string());
				TerminateProcess(process, 0);
				WaitForSingleObject(process, 5000);
				closed++;
			}
			CloseHandle(process);
		}
		CloseHandle(snapshot);
#else
		(void)appDir;
		(void)except;
#endif
		return closed;
	}

	bool StartProgram(const fs::path& appDir, const std::string& exeName, const std::string& args)
	{
#ifdef _WIN32
		auto exe = (appDir / exeName).wstring();
		std::wstring cmd = L"\"" + exe + L"\"" + (args.empty() ? L"" : L" " + Wide(args));
		STARTUPINFOW si{};
		si.cb = sizeof si;
		PROCESS_INFORMATION pi{};
		if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, appDir.wstring().c_str(), &si, &pi))
			return false;
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return true;
#else
		(void)appDir;
		(void)exeName;
		(void)args;
		return false;
#endif
	}
}
