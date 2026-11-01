// Full Tilt table launcher: shows every table found under "tables/" as a card and starts the game for it.
// Each table folder holds the table's .DAT (+ sound folder) and a table.ini:
//   [Table]  Name=  Game=  Data=ALIEN.DAT  Args=--table ALIEN.DAT  Preview=preview.bmp  Status=  Order=10
// Adding a new game later = adding a folder with a table.ini (the "library").
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

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
#endif

namespace fs = std::filesystem;

struct Table
{
	std::string Name, Game, Data, Args, Preview, Status;
	int Order = 100;
	fs::path Folder;
	SDL_Texture* Texture = nullptr;
	bool DataFound = false;
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
	{"HERO.DAT", "Hero", "Captain Hero", "Full Tilt! 2", "Work in progress - launch rolls back", true, 20},
	{"MAD.DAT", "Mad", "Mad Scientist", "Full Tilt! 2", "Playable - simple rules", true, 30},
	{"CADET.DAT", "SpaceCadet", "Space Cadet", "Full Tilt! Pinball", "Playable - full original rules", false, 40},
	{"DRAGON.DAT", "DragonsKeep", "Dragon's Keep", "Full Tilt! Pinball", "Work in progress - ball won't launch", true, 50},
	{"PIRATES.DAT", "Skulduggery", "Skulduggery", "Full Tilt! Pinball", "Work in progress - may freeze on load", true, 60},
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

static void OpenFolder(const fs::path& folder)
{
#ifdef _WIN32
	ShellExecuteA(nullptr, "open", folder.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
	std::string url = "file://" + folder.string();
	SDL_OpenURL(url.c_str());
#endif
}

static std::string Launch(const fs::path& exe, const Table& t)
{
	printf("Launching %s in %s: %s %s\n", t.Name.c_str(), t.Folder.string().c_str(), exe.string().c_str(), t.Args.c_str());
	fflush(stdout);
	if (!fs::exists(exe))
		return "Game program not found: " + exe.string();
	if (!t.DataFound)
		return "Table data not found: " + (t.Folder / t.Data).string();
#ifdef _WIN32
	std::string cmd = "\"" + exe.string() + "\" " + t.Args;
	std::string dir = t.Folder.string();
	STARTUPINFOA si{};
	si.cb = sizeof si;
	PROCESS_INFORMATION pi{};
	if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
		return "Could not start the game (error " + std::to_string(GetLastError()) + ")";
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
#else
	if (fork() == 0)
	{
		if (chdir(t.Folder.c_str()) != 0)
			_exit(1);
		std::vector<std::string> parts;
		std::string part;
		for (char c : t.Args + " ")
			if (c == ' ') { if (!part.empty()) parts.push_back(part); part.clear(); }
			else part += c;
		std::vector<char*> argv{const_cast<char*>(exe.c_str())};
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
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return 1;
	char* base = SDL_GetBasePath();
	fs::path basePath = base ? base : "./";
	SDL_free(base);
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
				if (entry.is_directory(ec))
					tables.push_back(ReadTable(entry.path()));
		std::sort(tables.begin(), tables.end(), [](const Table& a, const Table& b)
		{
			return a.Order != b.Order ? a.Order < b.Order : a.Name < b.Name;
		});
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
		             ImGuiWindowFlags_NoBringToFrontOnFocus);

		ImGui::SetWindowFontScale(1.6f);
		ImGui::TextColored({1.0f, 0.82f, 0.3f, 1}, "FULL TILT! PINBALL");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextDisabled("Table library  -  click a table or use the arrow keys and Enter.  Esc quits.");
		ImGui::Spacing();
		if (ImGui::Button("Open \"Put game files here\" folder"))
			OpenFolder(dropDir);
		ImGui::SameLine();
		if (ImGui::Button("Scan for new game files"))
			scan();
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

		// Size the cards so every table fits on screen without scrolling
		int columns = 3;
		int rows = std::max(1, static_cast<int>((tables.size() + columns - 1) / columns));
		float textH = ImGui::GetTextLineHeightWithSpacing() * 3 + 16;
		float availH = h - ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing() * 2 - 20;
		float imgH = std::min((availH / rows) - textH, (w - 48 - 24 * (columns - 1)) / static_cast<float>(columns) * 0.75f - 8);
		imgH = std::max(imgH, 90.0f);
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
				message = Launch(gameExe, t);
				if (message.empty())
					SDL_MinimizeWindow(window);
			}
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cardW);
			ImGui::Text("%s", t.Name.c_str());
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
		ImGui::TextDisabled("In a table: F2 new game, Space plunger, Z / slash flippers, X . Up nudge, F3 pause, F4 fullscreen.");
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
