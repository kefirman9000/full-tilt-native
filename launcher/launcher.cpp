// Full Tilt table launcher: shows every table found under "tables/" as a card and starts the game for it.
// Each table folder holds the table's .DAT (+ sound folder) and a table.ini:
//   [Table]  Name=  Game=  Data=ALIEN.DAT  Args=--table ALIEN.DAT  Preview=preview.bmp  Status=  Order=10
// Adding a new game later = adding a folder with a table.ini (the "library").
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "updater.h"
#include "../SpaceCadetPinball/ft_log.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

#ifdef _WIN32
static HANDLE RunningGame = nullptr;
#else
static pid_t RunningGame = 0;
#endif

struct Table
{
	std::string Name, Game, Data, Args, Preview, Status, Engine;
	int Order = 100;
	fs::path Folder;
	SDL_Texture* Texture = nullptr;
	bool DataFound = false;
	long long HighScore = 0;
	std::string HighScoreName;
};

static std::string Trim(const std::string& s)
{
	auto b = s.find_first_not_of(" \t\r\n");
	auto e = s.find_last_not_of(" \t\r\n");
	return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

static Table ReadTable(const fs::path& folder)
{
	Table t;
	t.Folder = folder;
	t.Name = folder.filename().string();
	std::ifstream ini(folder / "table.ini");
	std::string line;
	while (std::getline(ini, line))
	{
		auto eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		auto key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
		if (key == "Name") t.Name = value;
		else if (key == "Game") t.Game = value;
		else if (key == "Data") t.Data = value;
		else if (key == "Args") t.Args = value;
		else if (key == "Preview") t.Preview = value;
		else if (key == "Status") t.Status = value;
		else if (key == "Order") t.Order = std::atoi(value.c_str());
		else if (key == "Engine") t.Engine = value;
	}
	if (t.Data.empty())
		for (auto& entry : fs::directory_iterator(folder))
		{
			auto ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
			if (ext == ".DAT")
				t.Data = entry.path().filename().string();
		}
	t.DataFound = !t.Data.empty() && fs::exists(folder / t.Data);
	return t;
}


// ---------------------------------------------------------------------------------------------------------------
// Importer: everything dropped into "Put game files here" (whole install folders are fine) is searched for table
// data (.DAT files with a PARTOUT header). Each table found is copied with its sound folder and icon into
// tables/<Folder>/, and gets a table.ini if it has none. Originals are left untouched.
// ---------------------------------------------------------------------------------------------------------------
struct KnownTable
{
	const char* Dat;
	const char* Folder;
	const char* Name;
	const char* Game;
	const char* Status;
	bool UseTableArg; // false = the engine's default table search (Space Cadet runs with its full original rules)
	int Order;
};

static const KnownTable KnownTables[] =
{
	{"ALIEN.DAT", "Alien", "Alien Daze", "Full Tilt! 2", "Playable - simple rules", true, 10},
	{"HERO.DAT", "Hero", "Captain Hero", "Full Tilt! 2", "Playable - simple rules", true, 20},
	{"MAD.DAT", "Mad", "Mad Scientist", "Full Tilt! 2", "Playable - simple rules", true, 30},
	{"CADET.DAT", "SpaceCadet", "Space Cadet", "Full Tilt! Pinball", "Playable - full original rules", false, 40},
	{"DRAGON.DAT", "DragonsKeep", "Dragon's Keep", "Full Tilt! Pinball", "Playable - simple rules", true, 50},
	{"PIRATES.DAT", "Skulduggery", "Skulduggery", "Full Tilt! Pinball", "Playable - simple rules", true, 60},
	{"PINBALL.DAT", "SpaceCadetWindows", "3D Pinball Space Cadet", "Windows XP Pinball", "Playable - full original rules", false, 45},
};

static std::string Upper(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return s;
}

static bool IsTableData(const fs::path& file)
{
	std::ifstream in(file, std::ios::binary);
	char header[8]{};
	in.read(header, 7);
	return std::strncmp(header, "PARTOUT", 7) == 0;
}

// Full Tilt 1 icons say they use 257 colours, which image loaders reject; fix that in the copy.
static void CopyIcon(const fs::path& from, const fs::path& to)
{
	std::ifstream in(from, std::ios::binary);
	std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (data.size() > 54 && data[0] == 'B' && data[1] == 'M')
	{
		uint32_t used;
		std::memcpy(&used, &data[46], 4);
		if (used > 256)
		{
			used = 256;
			std::memcpy(&data[46], &used, 4);
			uint32_t important = 0;
			std::memcpy(&data[50], &important, 4);
		}
	}
	std::ofstream out(to, std::ios::binary);
	out.write(data.data(), static_cast<std::streamsize>(data.size()));
}


// Addiction Pinball (Team17, 1998): its own engine and table format (LZPK-packed "table.dat" in Worms\ and Rally\).
// It can't run on the Full Tilt engine yet; the install is stored in tables\_AddictionPinball and each table gets a card
// (and a Music folder for its CD tracks) so it's ready when its engine is added.
static bool IsAddictionTable(const fs::path& file)
{
	std::ifstream in(file, std::ios::binary);
	char header[5]{};
	in.read(header, 4);
	return std::strncmp(header, "LZPK", 4) == 0;
}

static void ImportAddiction(const fs::path& installDir, const fs::path& tablesDir, std::string& report, int& imported)
{
	std::error_code ec;
	auto store = tablesDir / "_AddictionPinball";
	if (!fs::exists(store / "Pinball.exe", ec))
	{
		fs::copy(installDir, store, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
		report += (report.empty() ? "" : ", ") + std::string("Addiction Pinball");
		imported++;
	}
	struct { const char* folder; const char* sub; const char* name; int order; } addictionTables[] =
	{
		{"AddictionWorms", "Worms", "Worms Pinball", 200},
		{"AddictionRally", "Rally", "World Rally Fever", 210},
	};
	for (auto& a : addictionTables)
	{
		if (!fs::exists(store / a.sub / "table.dat", ec))
			continue;
		auto folder = tablesDir / a.folder;
		if (fs::exists(folder / "table.ini", ec))
			continue;
		fs::create_directories(folder, ec);
		std::ofstream ini(folder / "table.ini");
		ini << "[Table]\nName=" << a.name << "\nGame=Addiction Pinball\nData=../_AddictionPinball/" << a.sub
			<< "/table.dat\nEngine=addiction\nStatus=Not playable yet - needs its own engine\nOrder=" << a.order << "\n";
	}
}

static int ImportGameFiles(const fs::path& dropDir, const fs::path& tablesDir, std::string& report)
{
	std::error_code ec;
	fs::create_directories(dropDir, ec);
	fs::create_directories(tablesDir, ec);
	int imported = 0;
	for (auto it = fs::recursive_directory_iterator(dropDir, fs::directory_options::skip_permission_denied, ec);
	     it != fs::recursive_directory_iterator(); it.increment(ec))
	{
		if (ec || !it->is_regular_file(ec) || Upper(it->path().extension().string()) != ".DAT")
			continue;
		auto dat = it->path();
		if (Upper(dat.filename().string()) == "TABLE.DAT" && IsAddictionTable(dat))
		{
			ImportAddiction(dat.parent_path().parent_path(), tablesDir, report, imported);
			continue;
		}
		auto datName = Upper(dat.filename().string());
		if (datName == "DEMO.DAT" || !IsTableData(dat))
			continue;

		const KnownTable* known = nullptr;
		for (auto& k : KnownTables)
			if (datName == k.Dat)
				known = &k;
		auto stem = dat.stem().string();
		auto target = tablesDir / (known ? known->Folder : stem);
		auto targetDat = target / dat.filename();
		if (fs::exists(targetDat, ec) && fs::file_size(targetDat, ec) == fs::file_size(dat, ec))
			continue; // already imported

		fs::create_directories(target, ec);
		fs::copy_file(dat, targetDat, fs::copy_options::overwrite_existing, ec);
		for (auto& sibling : fs::directory_iterator(dat.parent_path(), ec))
		{
			auto name = Upper(sibling.path().filename().string());
			if (sibling.is_directory(ec) && name == "SOUND")
				fs::copy(sibling.path(), target / sibling.path().filename(),
				         fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
			else if (sibling.is_regular_file(ec) && name == Upper(stem) + ".BMP")
				CopyIcon(sibling.path(), target / "icon.bmp");
		}
		if (!fs::exists(target / "table.ini", ec))
		{
			std::ofstream ini(target / "table.ini");
			ini << "[Table]\n";
			ini << "Name=" << (known ? known->Name : stem) << "\n";
			ini << "Game=" << (known ? known->Game : "Other") << "\n";
			ini << "Data=" << dat.filename().string() << "\n";
			ini << "Args=" << (known && !known->UseTableArg ? "" : "--table " + dat.filename().string()) << "\n";
			ini << "Preview=preview.bmp\n";
			ini << "Status=" << (known ? known->Status : "Untested") << "\n";
			ini << "Order=" << (known ? known->Order : 100) << "\n";
		}
		report += (report.empty() ? "" : ", ") + std::string(known ? known->Name : stem);
		imported++;
	}
	return imported;
}


// Every table gets a Music folder for CD-audio replacement files (see the README written into it)
static void EnsureMusicFolder(const fs::path& tableFolder, const std::string& tableName)
{
	std::error_code ec;
	auto music = tableFolder / "Music";
	if (fs::exists(music / "README.txt", ec))
		return;
	fs::create_directories(music, ec);
	std::ofstream readme(music / "README.txt");
	readme << "MUSIC FOR " << tableName << "\n"
		"==========\n"
		"Put this table's CD audio tracks here (mp3, ogg, wav or flac) to play them instead of the MIDI music.\n"
		"Files play in name order: the 1st file replaces the table's 1st music track, the 2nd the 2nd, and so on.\n"
		"Name them so they sort right, e.g. 01.mp3, 02.mp3, 03.mp3.\n"
		"Tracks without a file keep the original MIDI music. Empty folder = original MIDI music.\n"
		"FullTiltEngine.log (main folder) lists which file or MIDI track each music slot used.\n";
}


// High scores live in the engine's settings file (%APPDATA%\SpaceCadetPinball\imgui_pb.ini): "ALIEN.DAT 0.Score=..."
// for extra tables, plain "0.Score" for Space Cadet.
static void LoadHighScores(std::vector<Table>& tables)
{
	char* pref = SDL_GetPrefPath("", "SpaceCadetPinball");
	if (!pref)
		return;
	fs::path ini = fs::path(pref) / "imgui_pb.ini";
	SDL_free(pref);
	std::ifstream in(ini);
	std::vector<std::pair<std::string, std::string>> values;
	std::string line;
	while (std::getline(in, line))
	{
		auto eq = line.find('=');
		if (eq != std::string::npos)
			values.emplace_back(Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
	}
	auto find = [&](const std::string& key) -> std::string
	{
		for (auto& kv : values)
			if (kv.first == key)
				return kv.second;
		return "";
	};
	for (auto& t : tables)
	{
		std::string prefix;
		if (t.Args.find("--table") != std::string::npos)
		{
			prefix = t.Data;
			std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
			prefix += " ";
		}
		t.HighScore = std::atoll(find(prefix + "0.Score").c_str());
		t.HighScoreName = find(prefix + "0.Name");
	}
}

static std::string WithCommas(long long value)
{
	auto digits = std::to_string(value);
	std::string out;
	for (size_t i = 0; i < digits.size(); i++)
	{
		if (i && (digits.size() - i) % 3 == 0)
			out += ',';
		out += digits[i];
	}
	return out;
}

static void OpenFolder(const fs::path& folder)
{
#ifdef _WIN32
	ShellExecuteA(nullptr, "open", folder.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	std::string url = "file://" + folder.string();
	SDL_OpenURL(url.c_str());
#endif
}


// True once the game started from the launcher has closed (then the launcher window comes back)
static bool GameJustClosed()
{
#ifdef _WIN32
	if (RunningGame && WaitForSingleObject(RunningGame, 0) == WAIT_OBJECT_0)
	{
		CloseHandle(RunningGame);
		RunningGame = nullptr;
		return true;
	}
#else
	if (RunningGame > 0 && waitpid(RunningGame, nullptr, WNOHANG) == RunningGame)
	{
		RunningGame = 0;
		return true;
	}
#endif
	return false;
}


// "Report a problem": gathers the logs and settings into one zip next to the programs, so it can be attached to a
// GitHub issue or sent along. Game data is never included.
static std::string MakeProblemReport(const fs::path& basePath, const std::vector<Table>& tables)
{
	std::error_code ec;
	auto folder = basePath / "Problem report";
	fs::remove_all(folder, ec);
	fs::create_directories(folder, ec);
	for (auto& entry : fs::directory_iterator(basePath, ec))
	{
		auto ext = entry.path().extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (entry.is_regular_file(ec) && (ext == ".log" || entry.path().filename() == "FullTiltWindow.txt"))
			fs::copy_file(entry.path(), folder / entry.path().filename(), fs::copy_options::overwrite_existing, ec);
	}
	if (char* pref = SDL_GetPrefPath("", "SpaceCadetPinball"))
	{
		fs::copy_file(fs::path(pref) / "imgui_pb.ini", folder / "settings (imgui_pb.ini).txt", fs::copy_options::overwrite_existing, ec);
		SDL_free(pref);
	}
	{
		std::ofstream info(folder / "summary.txt");
		info << "Full Tilt Native " << updater::CurrentVersion << "\n";
		info << "Program folder: " << basePath.string() << "\n\nTables:\n";
		for (auto& t : tables)
			info << "  " << t.Name << " (" << t.Data << ") - " << (t.DataFound ? "data found" : "DATA MISSING") << " - " << t.Status << "\n";
	}
	auto zip = basePath / "FullTiltReport.zip";
	fs::remove(zip, ec);
#ifdef _WIN32
	auto tar = std::string(getenv("SystemRoot") ? getenv("SystemRoot") : "C:\\Windows") + "\\System32\\tar.exe";
	bool zipped = updater::RunHidden("\"" + tar + "\" -a -c -f \"" + zip.string() + "\" -C \"" + folder.string() + "\" *", basePath.string());
	if (zipped)
	{
		fs::remove_all(folder, ec);
		ShellExecuteA(nullptr, "open", "explorer.exe", ("/select,\"" + zip.string() + "\"").c_str(), nullptr, SW_SHOWNORMAL);
		return "Saved FullTiltReport.zip (logs and settings only). Attach it to a GitHub issue or send it along.";
	}
#endif
	OpenFolder(folder);
	return "Saved the \"Problem report\" folder (logs and settings only).";
}

static std::string Launch(const fs::path& exe, const Table& t, const std::string& windowSpot)
{
	ftlog::Line("Launching " + t.Name + " in " + t.Folder.string() + ": " + exe.string() + " " + t.Args);
	if (t.Engine == "addiction")
		return t.Name + " can't be played yet: Addiction Pinball needs its own engine (planned). Its files are stored and ready.";
	if (!fs::exists(exe))
		return "Game program not found: " + exe.string();
	if (!t.DataFound)
		return "Table data not found: " + (t.Folder / t.Data).string();
#ifdef _WIN32
	std::string cmd = "\"" + exe.string() + "\" " + t.Args + " --from-launcher --window " + windowSpot;
	std::string dir = t.Folder.string();
	STARTUPINFOA si{};
	si.cb = sizeof si;
	PROCESS_INFORMATION pi{};
	if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
		return "Could not start the game (error " + std::to_string(GetLastError()) + ")";
	CloseHandle(pi.hThread);
	if (RunningGame)
		CloseHandle(RunningGame);
	RunningGame = pi.hProcess;
#else
	auto pid = fork();
	if (pid > 0)
		RunningGame = pid;
	if (pid == 0)
	{
		if (chdir(t.Folder.c_str()) != 0)
			_exit(1);
		std::vector<std::string> parts;
		std::string part;
		for (char c : t.Args + " ")
			if (c == ' ') { if (!part.empty()) parts.push_back(part); part.clear(); }
			else part += c;
		std::vector<char*> argv{const_cast<char*>(exe.c_str())};
		parts.push_back("--from-launcher");
		parts.push_back("--window");
		parts.push_back(windowSpot);
		for (auto& p : parts) argv.push_back(p.data());
		argv.push_back(nullptr);
		execv(exe.c_str(), argv.data());
		_exit(1);
	}
#endif
	return "";
}

int main(int, char**)
{
	ftlog::Init("FullTiltPinball.log", (std::string("Full Tilt Native launcher ") + updater::CurrentVersion).c_str());
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		ftlog::Line(std::string("Could not start SDL: ") + SDL_GetError());
		return 1;
	}
	char* base = SDL_GetBasePath();
	fs::path basePath = base ? base : "./";
	SDL_free(base);
	updater::CleanupOldFiles(basePath);
	updater::CheckAsync();
#ifdef _WIN32
	fs::path gameExe = basePath / "FullTiltEngine.exe";
#else
	fs::path gameExe = basePath / "FullTiltEngine";
#endif

	auto window = SDL_CreateWindow("Full Tilt! Pinball - Table Library", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                               1120, 720, SDL_WINDOW_RESIZABLE);
	auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

	std::vector<Table> tables;
	auto tablesDir = basePath / "tables";
	auto dropDir = basePath / "Put game files here";
	auto loadTables = [&]()
	{
		for (auto& t : tables)
			if (t.Texture)
				SDL_DestroyTexture(t.Texture);
		tables.clear();
		std::error_code ec;
		if (fs::exists(tablesDir, ec))
			for (auto& entry : fs::directory_iterator(tablesDir, ec))
				if (entry.is_directory(ec) && entry.path().filename().string()[0] != '_')
					tables.push_back(ReadTable(entry.path()));
		for (auto& t : tables)
			for (auto& k : KnownTables)
				if (Upper(t.Data) == k.Dat)
					t.Status = k.Status; // status follows the program version, not the table.ini written at import
		std::sort(tables.begin(), tables.end(), [](const Table& a, const Table& b)
		{
			return a.Order != b.Order ? a.Order < b.Order : a.Name < b.Name;
		});
		for (auto& t : tables)
			EnsureMusicFolder(t.Folder, t.Name);
		LoadHighScores(tables);
		for (auto& t : tables)
			for (auto picture : {t.Preview, std::string("icon.bmp")})
			{
				if (picture.empty() || t.Texture)
					continue;
				if (auto surface = SDL_LoadBMP((t.Folder / picture).string().c_str()))
				{
					t.Texture = SDL_CreateTextureFromSurface(renderer, surface);
					SDL_FreeSurface(surface);
				}
			}
	};
	std::string importReport;
	auto scan = [&]()
	{
		std::string found;
		int count = ImportGameFiles(dropDir, tablesDir, found);
		importReport = count ? "Added: " + found : "";
		loadTables();
		ftlog::Line("Tables: " + std::to_string(tables.size()) + (count ? ", imported " + found : ""));
		for (auto& t : tables)
			if (!t.DataFound)
				ftlog::Line("Table " + t.Name + " is missing " + t.Data);
	};
	scan();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
	ImGui::StyleColorsDark();
	auto& style = ImGui::GetStyle();
	style.FrameRounding = 6;
	style.WindowPadding = {24, 20};
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);

	std::string message;
	bool running = true;
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				running = false;
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
				running = false;
			// Back from a game: refresh the high scores shown on the cards
			if (event.type == SDL_WINDOWEVENT &&
				(event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || event.window.event == SDL_WINDOWEVENT_RESTORED))
				LoadHighScores(tables);
		}
		if (GameJustClosed())
		{
			ftlog::Line("Game closed, back to the table library");
			// Reappear where the table window was
			std::ifstream spot(basePath / "FullTiltWindow.txt");
			int x, y, width, height;
			char comma;
			if (spot >> x >> comma >> y >> comma >> width >> comma >> height && width > 100 && height > 100)
			{
				SDL_SetWindowSize(window, width, height);
				SDL_SetWindowPosition(window, x, y);
			}
			SDL_ShowWindow(window);
			SDL_RestoreWindow(window);
			SDL_RaiseWindow(window);
			LoadHighScores(tables);
		}
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		int w, h;
		SDL_GetRendererOutputSize(renderer, &w, &h);
		float scale = std::max(1.0f, std::min(w / 1120.0f, h / 720.0f));
		ImGui::GetIO().FontGlobalScale = 1.4f * scale;
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize({static_cast<float>(w), static_cast<float>(h)});
		ImGui::Begin("library", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImGui::SetWindowFontScale(1.6f);
		ImGui::TextColored({1.0f, 0.82f, 0.3f, 1}, "FULL TILT! PINBALL");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextDisabled("Table library  -  click a table or use the arrow keys and Enter.  Esc quits.");
		ImGui::Spacing();
		{
			static bool dismissed = false;
			auto update = updater::Get();
			if (update.state == updater::State::Available && !dismissed)
			{
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.24f, 0.36f, 1));
				ImGui::BeginChild("update", {0, ImGui::GetTextLineHeightWithSpacing() * 2.6f}, true);
				ImGui::TextColored({1.0f, 0.82f, 0.3f, 1}, "Update available: %s (you have %s)", update.latestVersion.c_str(),
				                   updater::CurrentVersion);
				ImGui::SameLine();
				if (ImGui::Button("Update now"))
				{
					// The stand-alone updater closes us and the game so every file can be replaced, then reopens us
					if (updater::StartProgram(basePath, "FullTiltUpdate.exe", "--now"))
						running = false;
					else
						updater::InstallAsync(basePath);
				}
				ImGui::SameLine();
				if (ImGui::Button("Later"))
					dismissed = true;
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
			else if (update.state == updater::State::Downloading || update.state == updater::State::Installing)
				ImGui::TextColored({1.0f, 0.82f, 0.3f, 1}, "Updating: %s", update.message.c_str());
			else if (update.state == updater::State::Done)
			{
				if (updater::Restart(basePath))
					running = false;
				else
					ImGui::TextColored({0.55f, 0.85f, 0.55f, 1}, "Updated. Please start FullTiltPinball.exe again.");
			}
		}
		if (ImGui::Button("Open \"Put game files here\" folder"))
			OpenFolder(dropDir);
		ImGui::SameLine();
		if (ImGui::Button("Scan for new game files"))
			scan();
		ImGui::SameLine();
		if (ImGui::Button("Report a problem"))
			importReport = MakeProblemReport(basePath, tables);
		ImGui::SameLine();
		ImGui::TextDisabled("Drop game folders in, they get sorted.");
		if (!importReport.empty())
			ImGui::TextColored({0.55f, 0.85f, 0.55f, 1}, "%s", importReport.c_str());
		if (!message.empty())
			ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", message.c_str());
		if (tables.empty())
			ImGui::TextWrapped("No tables yet. Click \"Open \"Put game files here\" folder\", copy your Full Tilt game folders "
			                   "into it, then click \"Scan for new game files\".");
		ImGui::Separator();
		ImGui::Spacing();

		// Size the cards so every table fits on screen without scrolling: try 1-5 columns, keep the biggest cards
		float textH = ImGui::GetTextLineHeightWithSpacing() * 3 + 20;
		float footerH = ImGui::GetTextLineHeightWithSpacing() * 2 + 24;
		float availH = h - ImGui::GetCursorPosY() - footerH;
		float availW = w - 48.0f;
		int columns = 1;
		float imgH = 0;
		for (int c = 1; c <= 5; c++)
		{
			int r = std::max(1, static_cast<int>((tables.size() + c - 1) / c));
			float byHeight = availH / r - textH;
			float byWidth = (availW - 24.0f * (c - 1)) / c * 0.75f - 8;
			float candidate = std::min(byHeight, byWidth);
			if (candidate > imgH)
			{
				imgH = candidate;
				columns = c;
			}
		}
		imgH = std::max(imgH, 60.0f);
		float cardW = imgH * 4 / 3;
		for (size_t i = 0; i < tables.size(); i++)
		{
			auto& t = tables[i];
			if (i % columns)
				ImGui::SameLine(0, 24);
			ImGui::BeginGroup();
			ImGui::PushID(static_cast<int>(i));
			bool clicked;
			if (t.Texture)
			{
				int tw, th;
				SDL_QueryTexture(t.Texture, nullptr, nullptr, &tw, &th);
				float fit = std::min(cardW / tw, imgH / th);
				ImVec2 size{tw * fit, th * fit};
				ImVec2 cursor = ImGui::GetCursorPos();
				ImGui::Dummy({cardW + 8, imgH + 8});
				ImGui::SetCursorPos({cursor.x + (cardW - size.x) / 2, cursor.y + (imgH - size.y) / 2});
				clicked = ImGui::ImageButton("card", t.Texture, size);
				ImGui::SetCursorPos({cursor.x, cursor.y + imgH + 12});
			}
			else
				clicked = ImGui::Button(t.Name.c_str(), {cardW + 8, imgH + 8});
			if (clicked)
			{
				int wx, wy, ww, wh;
				SDL_GetWindowPosition(window, &wx, &wy);
				SDL_GetWindowSize(window, &ww, &wh);
				message = Launch(gameExe, t, std::to_string(wx) + "," + std::to_string(wy) + "," + std::to_string(ww) + "," +
				                 std::to_string(wh));
				if (!message.empty())
					ftlog::Line("Launch failed: " + message);
				if (message.empty())
					SDL_HideWindow(window); // one window: the table takes this spot until it closes
			}
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cardW);
			ImGui::Text("%s", t.Name.c_str());
			if (t.HighScore > 0)
				ImGui::TextDisabled("%s  -  High score %s %s", t.Game.c_str(), WithCommas(t.HighScore).c_str(),
				                    t.HighScoreName.c_str());
			else
				ImGui::TextDisabled("%s", t.Game.c_str());
			if (!t.DataFound)
				ImGui::TextColored({1, 0.45f, 0.4f, 1}, "Missing %s", t.Data.c_str());
			else if (t.Status.rfind("Playable", 0) == 0)
				ImGui::TextColored({0.55f, 0.85f, 0.55f, 1}, "%s", t.Status.c_str());
			else
				ImGui::TextColored({0.95f, 0.75f, 0.35f, 1}, "%s", t.Status.c_str());
			ImGui::PopTextWrapPos();
			ImGui::PopID();
			ImGui::EndGroup();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("F2 new game  Space plunger  Z / or Shift flippers  F3 pause  F4 fullscreen  F12 back here");
		{
			auto update = updater::Get();
			ImGui::TextDisabled("Version %s", updater::CurrentVersion);
			ImGui::SameLine();
			if (update.state != updater::State::Checking && update.state != updater::State::Downloading &&
				update.state != updater::State::Installing && ImGui::SmallButton("Check for updates"))
				updater::CheckAsync();
			if (!update.message.empty() && update.state != updater::State::Available)
			{
				ImGui::SameLine();
				if (update.state == updater::State::Failed)
					ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", update.message.c_str());
				else
					ImGui::TextDisabled("%s", update.message.c_str());
			}
			ImGui::SameLine();
			ImGui::TextDisabled("| Problems? Send the .log files");
		}
		ImGui::End();

		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, 18, 18, 24, 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	for (auto& t : tables)
		if (t.Texture)
			SDL_DestroyTexture(t.Texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	SDL_SetMainReady();
	return main(0, nullptr);
}
#endif
