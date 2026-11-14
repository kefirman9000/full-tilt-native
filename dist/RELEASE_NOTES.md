**Full Tilt Native**: Full Tilt! Pinball tables running natively on Windows 10/11 (64-bit), with no original game programs involved.

**What's in it**
- `FullTiltPinball.exe` — table library launcher (a card per table)
- `FullTiltEngine.exe` — the game engine (built on the SpaceCadetPinball decompilation)

**Game data is not included.** Copy your original game folders into `Put game files here` and start `FullTiltPinball.exe`: tables are found and sorted automatically.

**New in 0.10.0: Alien Daze plays by its real rules**
Alien Daze's own rules were recovered from Alien.exe and ported (ft2/alien_rules.md has the details):
- **Five missions**: Make Crop Circles, Abduct Humans, Re-capture Bigfoot, Take a Joy Ride and Re-assemble
  Abductee. The next one flashes; shoot the cornfield's center hole to start it (the landing ramp picks another).
  Each has its own shots and texts: crop circles in the cornfield holes, 17 abductees to beam up, Bigfoot walking
  between six spots leaving footprints, playing tag and buzzing landmarks, and putting an abductee back together
  (watch out for the horse head), then releasing or keeping it. Missions are timed (30/25/35/25 s; the last has no
  limit); the scoop on the right offers to extend (right flipper) or cancel (left flipper).
- **Land the spacecraft**: the four gear targets lower the landing gear, the landing ramp lands it (from
  "Emergency Landing -- Cow Crushed" to "Perfect Landing -- Cow Missed"), then the spacecraft lock opens: lock four
  balls for multiball.
- **Tractor beam**: 12 bumper hits switch it on and it lifts the ball up into the spacecraft.
- **Stun magnet**: the three space alien targets make it ready; it grabs the ball, the aim lights go round, and a
  flip fires the ball at the lit one.
- **Reserve power** for the outlane kickback (up to three charges), a **score multiplier** up to 3x from the UFO
  targets, **bonus hold** on the spiral ramp, an **extra ball** from the loop or the cornfield targets, **artifact
  trading** (right flipper yes, left no), bumper stages and the **jackpot**.
- **End-of-ball bonus** as on the original: missions, ship landed, abductees, artifacts traded, encasements.
- The side panel shows the mission, its time left, the gear, the lock and the jackpot.

**Fixes in 0.10.0**
- A ball could get wedged after a weak launch on Alien Daze (between the ray gun and its one-way gate) and never
  move again. Stuck balls on the Full Tilt 2 tables are now recognised (the check was set for Space Cadet's ten
  times smaller scale) and freed; a ball waiting at the plunger is still left alone.
- A kickout that the rules have switched off no longer pulls the ball to its center and holds it there.

**New in 0.9.2**
- **3-D Ultra Pinball (Sierra, 1995)** joins the library. Drop the `UPINBALL` folder from the CD, or the CD image
  (`.iso`), into **Put game files here** and click **Scan for new game files**. Then click **Compile 3-D Ultra
  Pinball** on its card (uses the same build tools as Epic Pinball; **Install build tools** covers both; takes a
  minute or two) and play. Controls match the Full Tilt tables (Shift flips, Space launches, F12 back to the library).
  The game's program is converted from your own copy on your computer; no game files are included.
  Not yet: its music (silent for now) and the intro/ending videos. This is the first release with it on Windows, so
  please report anything odd with **Report a problem**.

**Fixes in 0.9.1**
- **Captain Hero: ramp shots no longer roll back into the plunger lane.** The downtown ramp is also the shooter
  ramp; its return diverter stayed down, so a ball shot up the ramp from the playfield rode it all the way down to
  the plunger. The diverter now drops for each launch and comes back up 3 seconds later, turning those balls away.
- **Skulduggery: the ball no longer rolls off the upper playfield.** The blocker that closes off its bottom is
  raised as in the original, so the ball comes down through the exit ramp.
- **Safety net on every table:** if a ball ever gets outside the table, it is given back at the plunger ("BALL
  RETURNED") instead of being lost, and the log says where it got out.
- **Alien Daze's stun kickout** (and any pocket with no eject direction in its data) rolls the ball out toward the
  flippers instead of holding it.
- Self-test: Skulduggery's cannon lane counts as the plunger, so it no longer reports a false FAIL.

**New in 0.8.4: Epic Pinball (1993), native, whole table on screen**
Copy your Epic Pinball folder into `Put game files here`, click **Scan for new game files**, then **Compile Epic
Pinball tables** (bottom of the launcher; needs Python 3 and a C compiler, takes a few minutes). The original DOS
programs are recompiled to C on your computer from your own copy, then all 13 tables run natively without DOSBox.
The whole table fits on screen with no scrolling; **F11** switches to the original scrolling view. No DOS setup
screens. Sound is not in yet. The game's code is never included in this download (see `epic/README.md`).

**Also new in 0.9.0**
- **Transparent table background** (Options > Graphics > Backdrop): the black around the table art becomes
  see-through, so the ambient glow or the table's title screen shows behind the table, not only beside it.
- **Nudge with Z and /** when the flippers are on Left/Right Shift (Options > Flipper Keys). The nudge keys can
  also be set to any key in Options > Player Controls (F8).
- **The side panel switches itself on once** after updating (settings from older versions had it off); turn it off
  in Options > Graphics if you prefer, and it stays off.
- **Fixed: a crash after a minute without pressing a key** on every table except Space Cadet.
- **Self-test**: `FullTiltEngine --table HERO.DAT --selftest 120` plays a table by itself and reports PASS/FAIL;
  `selftest.ps1` does it for every table in the library.

**Captain Hero in 0.9.0: villain battles**
- **Battle the villains.** Shoot the scoop: the display offers a villain (left flipper for the next, right
  flipper to fight). Quake, Mad Scientist, Mystman, Slime Man, Mr. Pling and Evil Twin each have their own shots
  to beat them, read from the original table program. Beating one pays the jackpot. Messages are the table's
  own, read from hero.exe ("Mystman Captured, Jackpot Awarded!!!").
- **Skill shot awards as on the original**: jackpot increase, forcefields (the ball saver), 1,000,000 or an extra
  ball.
- **Fixed: the ball that never drained.** The hole between the flippers is the table's ball saver; it was always
  open, so most balls came straight back. It now opens only for the ball save time after a launch and from the
  skill shot's forcefields.
- The henchmen now only come out in Mystman's battle.

**Mad Scientist: real point values** read from mad.exe; its beaker bumpers build up to 10,000.

**Dot-matrix display everywhere** (Options > Graphics > Dot Matrix Display > "In the side panel and over every
table"): Space Cadet and the Full Tilt 1 tables show it in their message box, Full Tilt 2 tables without the side
panel beside or above the table. The display animates big moments (flash and chasing border), cycles through the
table's name, high scores and PRESS F2 between games, and asks for your name on a new high score. The side panel
lists the last few messages.

**New in 0.9.0: side panel, dot-matrix display, Table Guide** (Full Tilt 2 tables)
- **Side panel on by default** (Options > Graphics > Side Score Panel): the table's title screen, a dot-matrix
  display with the score and the rules' messages, every player's score, extra ball, the table's best score and
  what the rules are doing now (bonus multiplier, lit pockets, Captain Hero's pop value, henchmen, skill shot).
  The title screen is read from the table's own program (Alien.exe, hero.exe, mad.exe), so nothing extra to copy.
  Put a `panel.bmp` in a table folder to use your own picture. Framed like the Full Tilt 1 panels: a bevelled
  gold border, the picture across the top fading into a dark, blurred backdrop of its own colours, and the score,
  message and high score in black inset boxes.
- **No more black boxes on the table.** The score and message areas used to clear themselves to black over the
  table art. They now show the art underneath, and move to the side panel while it is on.
- **Dot-matrix display** (Options > Graphics, on by default; off shows plain text in the panel).
- **Help > Table Guide**: how the table in play scores, and your statistics for it (games, best, average, last
  game, time played).
- **Practice mode** (Options > Table Rules): every lost ball comes back. Practice games are not high scores or
  statistics.
- **Controller rumble** (Options, on by default): bumpers, slingshots, flippers, nudges, drains and tilt.
- **Upscale Table Art** has two new smooth settings (2x / 4x smooth) with anti-aliased diagonals.

**Captain Hero in 0.9.0: real scoring and the skill shot**
- **Real point values.** Captain Hero's scores were read from the original table program: lanes 5,000-20,000,
  targets and drop targets 5,000-100,000 a hit, scoops 25,000-70,000, mid-table pockets 50,000, and so on.
  Scores now run at the original table's scale. (The original's bigger awards for lit targets and finished
  banks come with the table's modes.)
- **Pop bumpers build up**: 1,000 a hit, 1,000 more every 8 hits, up to 5,000; back to 1,000 for each new ball.
- **Henchmen take five hits**, as on the original: 10,000, 20,000, 30,000, 40,000; the fifth wins Mystman's battle.
- **Skill shot.** Pull the plunger back a little (about a quarter of a second) so the ball only just makes it to
  the top of the shooter lane: 500,000, sometimes 1,000,000, and once a game it can light an extra ball. A tap
  is too soft: the ball comes back down the lane, so just plunge again. A harder pull plays as before.

**Captain Hero fixes in 0.8.9**
- **Enemies only show up when the rules send them out.** The four henchmen on the playfield stood there from the
  first ball. They now start off the table. Complete a set of lanes, targets or drop targets and one of them comes
  out. Hit him three times to knock him out for 25,000 points (times the bonus multiplier).
- **No more balls caught in the middle of the table.** Five invisible pockets in the open playfield kept grabbing
  the ball and spitting it back out. They now stay closed like the other mid-table magnets, and open with them for
  20 seconds when you complete a set.
- **The launch follows the plunger** (all Full Tilt 2 tables): a quick tap is a gentle shot (45% strength) and a
  full pull is the table's full strength. Before, every launch went at 80-100%. Even the weakest shot gets the
  ball out of the shooter lane.
- Pop-up targets that only appear in table modes no longer come up when the next player starts.

**Fixes in 0.8.8: Full Tilt 2 table physics** (Captain Hero, Alien Daze, Mad Scientist)
- **Pockets eject the ball at their real strength.** Many Full Tilt 2 pockets give their eject direction as a long
  line rather than a unit direction, and the engine multiplied the throw speed by that length: Captain Hero's
  pockets fired the ball at the engine's top speed, straight past the flippers. The direction is now normalised.
- **Rolling friction from the table itself.** Full Tilt 2 tables store their own drag setting, which was ignored
  (all of them used Space Cadet's). The ball now slows down the way the table was tuned, so it no longer rattles
  around the playfield at full speed.
- A damaged or incomplete table `.dat` file now shows the "game data missing" message instead of crashing.

**Fix in 0.8.7: ball shadow and reflection hide where the ball hides**
The shadow and gloss were drawn on top of the finished picture, so they showed over ramps, wire rails and plastics
even when the ball went underneath. They are now part of the table picture and use the ball's own depth test:
wherever something covers the ball, it covers its shadow too.

**New in 0.8.6: graphics enhancements for the Full Tilt tables** (Options > Graphics)
- **Ball Shadow & Reflection** (on by default): a soft shadow under the ball and a faint gloss on the playfield.
- **Light Pools** (Off / Soft / Strong, default Soft): lit lamps cast light of their own colour onto the artwork
  around them, on top of the existing Glow.
- **Upscale Table Art** (Off / 2x / 4x): Scale2x pixel-art upscaling rounds off jagged edges and diagonals without
  blurring, for sharper tables on big screens.

**New in 0.8.5: launcher**
- **Install build tools** button: installs Python 3, the GCC compiler and capstone for your user (winget), so
  compiling Epic Pinball needs no manual setup.
- Compiling Epic Pinball shows a **progress bar**.
- **Hide or reorder tables**: right-click a table card to move it left/right, to the start or end, or hide it.
  "Show hidden tables" brings hidden ones back. The layout is kept in `tables/library.ini`.
- Bottom-bar buttons wrap onto a second line instead of running off narrow windows.

**Launcher (0.8.4)**: the "Put game files here" and "Scan for new game files" buttons moved to the bottom bar, next to the
new Epic Pinball button.

**Fixes in 0.8.3**
- **Table library no longer breaks after adding Marble Drop**: its level files could each become a table card,
  shrinking the library to a column of tiny cards that ran off the window. Marble Drop is now recognised by its
  install (COREA.DAT / COREB.DAT), level cards left by an earlier scan are hidden, and the cards scroll instead of
  pushing the footer off screen.
- **Glow only on lights**: Options > Graphics > Glow now blooms lit lights, lane lights and bumper caps only.
  Flippers and the ball (shiny chrome) no longer glow.
- **No more stall when turning music on**: starting MIDI opens Windows' synth, which could freeze the game for up
  to a second. Music now starts on its own thread, and turning it off (or pausing) pauses it instead of closing the
  synth, so turning it back on is instant.
- **Dragon's Keep and Skulduggery show the real ball**: their ball pictures hide a smiley face behind each frame
  (an easter egg), which was replacing the ball. The smiley is now an option: Options > Ball > Smiley.
- **Real point values on Dragon's Keep and Skulduggery**, read from the original 16-bit table programs.
- A timer bug from the original decompilation (cancelling timers by callback could drop other running timers) is fixed.

**New in 0.8.3: Marble Drop marbles as balls**
Marble Drop (Maxis, 1997) was made by the same team on the same engine. Copy your Marble Drop folder into
`Put game files here` and its ten marbles (red, yellow, blue, orange, green, magenta, black, steel, glass, fire)
appear under Options > Ball on every table, scaled to that table's ball. Its level files are stored, not turned
into table cards.

**Fixes in 0.8.1 / 0.8.2**
- **Mad Scientist's plunger tube only bubbles while you pull the plunger back**, not all the time.
- **Pockets spit the ball out properly** (Mad Scientist, and pockets on every table that had the same problem): many
  kickouts and sinks describe their eject as a line from the pocket to where the ball should go. That line was
  ignored, so the ball was thrown with no direction. It is now the throw direction.
- **Captain Hero's mid-table magnet pockets no longer grab the ball just from rolling over them.** The original rules
  only switch them on during modes; they now stay closed and open for 20 seconds when you complete a set or bank.

**New in this version**
Every table that doesn't have its original rules yet (all but Space Cadet) gets real pinball goals, built from its
own parts (Options > Table Rules > Enhanced Rules, on by default):
- **Drop target banks**: knock down a whole bank for a bonus, then the bank pops back up.
- **Lane and target sets**: hit every rollover lane / standup in a set for a bonus; the lane lights reset.
- **Bonus multiplier**: each completed set or bank raises it, up to x5.
- **End-of-ball bonus**: every scoring hit adds to the bonus, paid out times the multiplier when the ball drains.
- **Ball save**: lose the ball within 8 seconds of launching and you get it back once (Off / 5 / 8 / 15 seconds).
- **Extra ball** at 1,500,000 points (once per player per game), with "Shoot Again".
- **Light shows** on completions and extra balls, and an attract light show after the game ends.
- Messages on the table for all of these.


**Graphics** (Options > Graphics):
- **Glow**: lit lights and bright parts bloom softly (Off / Soft / Strong; Soft is on by default).
- **Brightness**: Normal / Bright / Brighter, for dark screens and dark tables.
- **Vignette**: slightly darker edges that pull the eye to the playfield (on by default).
- **Motion Trails**: the ball and flippers leave a short streak (off by default).

**Launcher**:
- **Report a problem** button: collects the logs and settings (never game files) into FullTiltReport.zip next to
  the programs and shows it in Explorer, ready to attach to a GitHub issue.

Also: Skulduggery's ball no longer falls out of the table, and the Full Tilt 2 tables play their sounds (0.7.1).

**Status**
- Alien Daze, Captain Hero, Mad Scientist, Dragon's Keep, Skulduggery: playable with simple placeholder rules
- Space Cadet: full original rules

If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
