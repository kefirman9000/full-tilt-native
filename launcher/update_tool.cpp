// FullTiltUpdate.exe: stand-alone updater that lives in the Full Tilt Native folder.
// Double-click it (or the launcher's "Update now" starts it with --now): it checks GitHub, closes the launcher and the
// game if they are running from this folder (their files can't be replaced while in use), installs the update and
// opens the launcher again.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "updater.h"
#include "../SpaceCadetPinball/ft_log.h"

#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
	ftlog::Init("FullTiltUpdate.log", (std::string("Full Tilt Native updater ") + updater::CurrentVersion).c_str());
	bool autoInstall = false;
#ifdef _WIN32
	autoInstall = strstr(GetCommandLineA(), "--now") != nullptr;
#else
	for (int i = 1; i < argc; i++)
		autoInstall |= std::strcmp(argv[i], "--now") == 0;
#endif
	(void)argc;
	(void)argv;
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return 1;
	char* base = SDL_GetBasePath();
	fs::path appDir = base ? base : "./";
	SDL_free(base);
	updater::CleanupOldFiles(appDir);

	auto window = SDL_CreateWindow("Full Tilt Native - Update", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 620, 300, 0);
	auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().FontGlobalScale = 1.4f;
	ImGui::StyleColorsDark();
	ImGui::GetStyle().FrameRounding = 6;
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);

	updater::CheckAsync();
	bool installing = false, running = true, reopened = false;
	auto install = [&]()
	{
		installing = true;
		updater::CloseRunningPrograms(appDir, "");
		updater::InstallAsync(appDir);
	};

	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				running = false;
		}
		auto status = updater::Get();
		if (autoInstall && !installing && status.state == updater::State::Available)
			install();
		if (status.state == updater::State::Done && !reopened)
		{
			reopened = true;
			updater::StartProgram(appDir, "FullTiltPinball.exe", "");
			running = false;
		}

		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
		int w, h;
		SDL_GetRendererOutputSize(renderer, &w, &h);
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize({static_cast<float>(w), static_cast<float>(h)});
		ImGui::Begin("update", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
		ImGui::TextColored({1.0f, 0.82f, 0.3f, 1}, "FULL TILT NATIVE - UPDATE");
		ImGui::TextDisabled("Installed version: %s", updater::CurrentVersion);
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::PushTextWrapPos(0);
		switch (status.state)
		{
		case updater::State::Available:
			ImGui::Text("Version %s is available.", status.latestVersion.c_str());
			ImGui::TextDisabled("The launcher and any running table will be closed, then reopened after the update.");
			ImGui::Spacing();
			if (!installing && ImGui::Button("Update now"))
				install();
			break;
		case updater::State::UpToDate:
			ImGui::Text("You have the latest version.");
			ImGui::Spacing();
			if (ImGui::Button("Open Full Tilt Pinball"))
			{
				updater::StartProgram(appDir, "FullTiltPinball.exe", "");
				running = false;
			}
			break;
		case updater::State::Failed:
			ImGui::TextColored({1, 0.45f, 0.4f, 1}, "%s", status.message.c_str());
			ImGui::Spacing();
			if (ImGui::Button("Try again"))
			{
				installing = false;
				updater::CheckAsync();
			}
			break;
		default:
			ImGui::Text("%s", status.message.empty() ? "Starting..." : status.message.c_str());
			break;
		}
		ImGui::PopTextWrapPos();
		ImGui::SetCursorPosY(h - ImGui::GetTextLineHeightWithSpacing() * 1.6f);
		ImGui::TextDisabled("Problems? Send FullTiltUpdate.log from this folder.");
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
