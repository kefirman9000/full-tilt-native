#pragma once

// Full Tilt 1-style side panel for the full-screen Full Tilt 2 tables (Options > Graphics > Side Score Panel):
// the table's art, a dot-matrix display (score, and the rules' messages as they come), the players' scores, ball,
// bonus, the table's best score and what the table rules are doing right now. Drawn in render::SidePanelRect.
class side_panel
{
public:
	static void Draw();
	// Redraw the table's own score and message boxes (after the panel is switched on or off)
	static void RefreshTableBoxes();
};
