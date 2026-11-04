#pragma once

// Self-test (--selftest SECONDS): the engine plays the table by itself, plunging at random strengths and flipping
// when a ball comes near a flipper, starts a new game after each game over, and at the end writes one line to the
// log and the console:
//   SELFTEST table=HERO.DAT seconds=120 games=2 balls=7 launches=9 longest_ball=41.3 result=PASS
// It fails (exit code 1) on a ball that stays in play longer than --selftest-ball-limit seconds (default 240:
// the self-test player is not good enough to keep a ball that long, so it means a ball that never drains), a ball
// that sits still away from the plunger for 25 seconds, or a game that never takes a ball. A crash writes its own
// CRASH line (see ft_log) and ends the program.
class selftest
{
public:
	static bool Active();
	static void Parse(const std::string& commandLine);
	// Every simulation update (pb::frame)
	static void Frame(float dtMilliSec);
	static int ExitCode();
};
