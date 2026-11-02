#pragma once
// Real point values recovered from the original table programs (rule table in each exe: component name,
// rule function, scores). Generated from Alien.exe; Captain Hero and Mad Scientist keep theirs inside the rule
// functions and are added as those are ported.
#include <vector>

struct ft2_score_entry
{
	const char* Table;
	const char* Component;
	std::vector<int> Scores;
};

static const ft2_score_entry ft2_scores[] =
{
	{"ALIEN.DAT", "a_bmpr01", {1500, 3000, 4500, 6000, 7500}},
	{"ALIEN.DAT", "a_bmpr02", {1500, 3000, 4500, 6000, 7500}},
	{"ALIEN.DAT", "a_bmpr03", {1500, 3000, 4500, 6000, 7500}},
	{"ALIEN.DAT", "a_bmpr04", {1500, 3000, 4500, 6000, 7500}},
	{"ALIEN.DAT", "a_rebo02", {350}},
	{"ALIEN.DAT", "a_rebo01", {350}},
	{"ALIEN.DAT", "a_drop07", {10000, 50000}},
	{"ALIEN.DAT", "a_drop06", {10000, 50000}},
	{"ALIEN.DAT", "a_drop05", {10000, 50000}},
	{"ALIEN.DAT", "a_drop01", {6000, 15000}},
	{"ALIEN.DAT", "a_drop02", {6000, 15000}},
	{"ALIEN.DAT", "a_drop09", {5000, 10000, 15000, 20000, 30000}},
	{"ALIEN.DAT", "a_drop10", {5000, 10000, 15000, 20000, 30000}},
	{"ALIEN.DAT", "a_drop11", {5000, 10000, 15000, 20000, 30000}},
	{"ALIEN.DAT", "a_drop12", {5000, 10000, 15000, 20000, 30000}},
	{"ALIEN.DAT", "a_drop13", {5000, 10000, 15000, 20000, 30000}},
	{"ALIEN.DAT", "a_drop14", {5000, 10000, 15000, 20000, 30000}},
	{"ALIEN.DAT", "a_roll01", {10000}},
	{"ALIEN.DAT", "a_roll05", {25000}},
	{"ALIEN.DAT", "a_roll02", {10000}},
	{"ALIEN.DAT", "a_roll03", {5000}},
	{"ALIEN.DAT", "a_roll04", {5000}},
	{"ALIEN.DAT", "a_spot04", {7500, 15000}},
	{"ALIEN.DAT", "a_spot03", {7500, 15000}},
	{"ALIEN.DAT", "a_spot02", {7500, 15000}},
	{"ALIEN.DAT", "a_spot01", {7500, 15000}},
	{"ALIEN.DAT", "a_spot05", {10000, 30000}},
	{"ALIEN.DAT", "a_spot06", {10000, 30000}},
	{"ALIEN.DAT", "a_spnr01", {1000}},
	{"ALIEN.DAT", "s_ramp01", {50000, 100000, 200000, 400000}},
	{"ALIEN.DAT", "s_ramp15", {25000, 50000, 100000, 250000, 500000}},
	{"ALIEN.DAT", "s_ramp13", {40000, 80000, 160000, 320000}},
};
