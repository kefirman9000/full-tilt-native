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

#ifdef _WIN32
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
	if (fs::exists(tablesDir))
		for (auto& entry : fs::directory_iterator(tablesDir))
			if (entry.is_directory())
				tables.push_back(ReadTable(entry.path()));
	std::sort(tables.begin(), tables.end(), [](const Table& a, const Table& b)
	{
		return a.Order != b.Order ? a.Order < b.Order : a.Name < b.Name;
	});
	for (auto& t : tables)
		if (!t.Preview.empty())
			if (auto surface = SDL_LoadBMP((t.Folder / t.Preview).string().c_str()))
			{
				t.Texture = SDL_CreateTextureFromSurface(renderer, surface);
				SDL_FreeSurface(surface);
			}

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
		if (!message.empty())
			ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", message.c_str());
		if (tables.empty())
			ImGui::TextWrapped("No tables found. Put table folders inside: %s", tablesDir.string().c_str());
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
				clicked = ImGui::ImageButton("card", t.Texture, {cardW, imgH});
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
