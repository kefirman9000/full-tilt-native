#pragma once
#include "gdrv.h"

enum class GameBindings;

struct SdlTickClock
{
	using duration = std::chrono::milliseconds;
	using rep = duration::rep;
	using period = duration::period;
	using time_point = std::chrono::time_point<SdlTickClock>;
	static constexpr bool is_steady = true;

	static time_point now() noexcept
	{
		return time_point{duration{SDL_GetTicks()}};
	}
};

struct SdlPerformanceClock
{
	using duration = std::chrono::duration<uint64_t, std::nano>;
	using rep = duration::rep;
	using period = duration::period;
	using time_point = std::chrono::time_point<SdlPerformanceClock>;
	static constexpr bool is_steady = true;

	static time_point now() noexcept
	{
		const auto freq = SDL_GetPerformanceFrequency();
		const auto ctr = SDL_GetPerformanceCounter();
		const auto whole = (ctr / freq) * period::den;
		const auto part = (ctr % freq) * period::den / freq;
		return time_point(duration(whole + part));
	}
};

struct WelfordState
{
	double mean;
	double M2;
	int64_t count;

	WelfordState() : mean(0.005), M2(0), count(1)
	{
	}

	void Advance(double newValue)
	{
		++count;
		auto delta = newValue - mean;
		mean += delta / count;
		M2 += delta * (newValue - mean); //M2n = M2n-1 + (Xn - AvgXn-1) * (Xn - AvgXn)
	}

	double GetStdDev() const
	{
		return std::sqrt(M2 / (count - 1)); // Sn^2 = M2n / (n - 1)
	}
};

// Turns real elapsed time into fixed-size simulation steps. What is left over after the steps, as a fraction of
// a step, says how far the picture should move on from the latest step (frame interpolation).
struct FixedStepAccumulator
{
	// Elapsed time (ms) not simulated yet. Below one step after Advance().
	double Pending = 0;

	void Reset() { Pending = 0; }

	// Adds elapsed time, at most maxElapsedMs so that a stall cannot cause a burst of updates,
	// and returns how many steps of stepMs to simulate now.
	int Advance(double elapsedMs, double stepMs, double maxElapsedMs)
	{
		Pending += std::min(elapsedMs, maxElapsedMs);
		int steps = 0;
		while (Pending >= stepMs)
		{
			Pending -= stepMs;
			steps++;
		}
		return steps;
	}

	// Progress from the latest step towards the next one, 0..1.
	float Alpha(double stepMs) const
	{
		return static_cast<float>(std::min(std::max(Pending / stepMs, 0.0), 1.0));
	}
};

class winmain
{
	using Clock = SdlPerformanceClock; // Or std::chrono::steady_clock.
	using DurationMs = std::chrono::duration<double, std::milli>;
	using TimePoint = std::chrono::time_point<Clock>;

public:
	static constexpr const char* Version = "2.1.1 DEV";
	static bool single_step;
	static SDL_Window* MainWindow;
	static SDL_Renderer* Renderer;
	static ImGuiIO* ImIO;
	static bool LaunchBallEnabled;
	static bool HighScoresEnabled;
	static bool DemoActive;
	static int MainMenuHeight;

	static int WinMain(LPCSTR lpCmdLine);
	static int event_handler(const SDL_Event* event);
	[[ noreturn ]] static void memalloc_failure();
	static int ProcessWindowMessages();
	static void a_dialog();
	static void end_pause();
	static void new_game();
	static void pause(bool toggle = true);
	static void Restart();
	static void UpdateFrameRate();
	static void HandleGameBinding(GameBindings binding, bool shortcut);
private:
	static int return_value;
	static int mouse_down, last_mouse_x, last_mouse_y;
	static bool no_time_loss, activated, bQuit, has_focus, DispGRhistory, DispFrameRate;
	static std::vector<float> gfrDisplay;
	static std::string FpsDetails, PrevSdlError;
	static bool restart;
	static bool ShowAboutDialog;
	static bool ShowImGuiDemo;
	static bool ShowSpriteViewer;
	static bool ShowExitPopup;
	static double UpdateToFrameRatio;
	static DurationMs TargetFrameTime;
	static struct optionsStruct& Options;
	static DurationMs SpinThreshold;
	static WelfordState SleepState;
	static unsigned PrevSdlErrorCount;
	static unsigned gfrOffset;
	static float gfrWindow;
	static int CursorIdleCounter;

	// True when frames are presented between fixed physics updates: interpolation is on and FPS is above UPS.
	static bool UseFrameInterpolation();
	static void RenderUi();
	static void RenderFrameTimeDialog();
	static void HybridSleep(DurationMs seconds);
	static void MainLoop();
	static void ImGuiMenuItemWShortcut(GameBindings binding, bool selected = false);
};
