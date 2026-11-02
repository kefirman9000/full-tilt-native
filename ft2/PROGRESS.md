# Full Tilt! 2 on the SpaceCadetPinball decomp - progress notes

Base: https://github.com/k4zmu2a/SpaceCadetPinball (commit cb9b7b8, Aug 2024). Apply with `git apply ft2_decomp_experiment.patch`.
Run: `FT2_DAT=HERO.DAT ./SpaceCadetPinball` (or ALIEN.DAT / MAD.DAT) with the .DAT next to the binary.

## Status
All three tables (Alien, Hero, Mad) now load every record, build every component, and run the game loop.
Tested ONLY headless on Linux (SDL dummy video/audio): each ran 40 s under AddressSanitizer with no errors, and a release
build kept running at steady CPU for 15 s. Nobody has SEEN a frame yet. Rendering, input, sound and ball behaviour are untested.
Rules are switched off for these tables (see below), so this is a physics sandbox, not a playable game.

## What was different about the FT2 tables (all found by reading the DATs, all fixed in the patch)
1. Table size: backgrounds are full screen (640x480 / 800x600 / 1024x768); the decomp hard-codes Cadet's smaller table size.
2. Spliced sprites: a jump in the run data is k*screenWidth + column offset (k can be 2+). The decomp assumed k == 1.
   General conversion: rows = round(stride / tableW); stride = rows * bmp.Width + (stride - rows*tableW). Same result for k == 1.
3. Score records are named score_box / ballcount_box / player_box (not score1 / ballcount1 / player_number1). Same layout + 1 extra short.
4. Sound records: 80 / 97 / 69 per table; the decomp caps at 64. Cap raised to 255.
5. Kicker attribute 410 is followed by 8 floats (looks like two groups of x, y, radius, 0). Meaning unknown; skipped as a block.
6. Collision walls (attribute 600) live in the group just BEFORE the component's group (`<name>` before `<name>_animation`;
   `<name>_walls` before a ramp). The table's component list points at the later group. Blockers/ramps fall back to group-1.
7. Ramps: attribute 1300 (plane) and 1301/1302 (side walls) have the same layout as Cadet; 1303 (Cadet's end wall) is absent,
   replaced by the polyline in the `_walls` group. 1301/1302 may be missing entirely, so they are now optional.
8. Kickouts: attribute 408 can be missing; ball Z defaults to 0. Lights may lack 900/901; default delay 0.1.
9. No text-box groups exist (info_text_box / mission_text_box); zero-size dummy text boxes are created instead.
10. Space Cadet's rules (control.cpp) link by Cadet component names and crash on these tables. For FT2_DAT, make_links /
    handler / table_control_handler do nothing, so there are no missions, scoring or light logic.

## 2026-11-01 corrections (first run with a real display)
- Earlier headless runs hid error dialogs. With a display, Alien and Mad stopped on loader errors.
- Point 6 above was wrong. Gate/blocker groups (e.g. a_gate01, loop_diverter, *_oneway_animation) are
  animation-only: no wall. Their wall is a separate oneway/wall object in table_objects. Ramp side walls
  are also separate wall objects (in Mad the unnamed group right AFTER a ramp; in Alien named s_rampNN groups).
  The group-1 fallbacks are removed (they built walls from the wrong group or duplicated walls).
- Dummy text boxes now use group -1 (group 0 has no visual and raised an error).
- Unknown object types: 1042 (Alien s_mag01), 1044 (Alien beamout/socket), 1045 (Mad bubbling_*, 65 of them,
  attrs 700/701/702/501). Currently skipped.
- tools/datx.py (detailed group dump), tools/objs.py (table_objects with types), tools/orphans.py.


## 2026-11-01 (later): playable sandbox
Run: `SpaceCadetPinball --table ALIEN.DAT` (or HERO/MAD/DRAGON/PIRATES) with the .DAT in the working folder.
Without `--table`, CADET.DAT runs Space Cadet with its full original rules.

Findings and fixes:
- White boxes beside sprites: empty pixels of spliced sprites (z 0xFFFF) were painted over empty screen (z 0xFFFF).
  zdrv::paint now skips z 0xFFFF source pixels. The FT2 "background" record is not a backdrop; FT2 starts from black.
- Score digits: FT2 fonts are ASCII tables starting at ' '; digit 0 is +16 glyphs.
- Message text: FT2 has no info/mission text boxes; messages go to the table's notify_box / mode_box areas.
- **World orientation**: Space Cadet's camera distance is negative (flips both screen axes); FT2's is positive.
  So the FT2 world is rotated 180 degrees vs Cadet: down the table is -Y. Gravity is inverted for FT2 layout tables.
  FT1 extra tables (Pirates/Dragon) have negative camera distance like Cadet.
- World scale ~10x Cadet (ball radius 3.3 vs 0.3; gravity mult 200 vs 25). Most engine constants derive from ball radius.
- Plunger: strength is attribute 402 (Alien 500, Hero 1000, Mad 600). Alien's ray-gun plunger is angled and fires the
  ball up-left through a one-way gate; FT2 launch pushes the resting ball away from the nearest plunger face.
- Gates made of two back-to-back one-ways (s_onew01/s_onew03 at Alien's lane exit): rules switch one off. Sandbox
  switches the later one off.
- Ball in a kickout sits at the kickout's Z (attribute 408 [3]): 0 for Alien's stun kickout (half sunk), -3.3 for s_kout01.
- Layout detection: a table with a `score_box` record is an FT2 full-screen table (pb::FT2FullScreen).
- Tools: tools/wallmap.py (SVG plot of walls), tools/hitwall.py (walls crossing a line), tools/at.py (sprites at a pixel).

Status: Alien and Mad play (launch, scoring, drain, next ball). Hero launches but the shot returns down the lane.
Dragon's Keep draws with its side panel, ball not launching. Skulduggery hangs on load. See ROADMAP.md.

## Not done / unknown
- Rules for each table (missions, lights, scoring, ramps' behaviour, multiball) live in each table's exe. Recovering them
  (Ghidra) is the big remaining job and has not been started.
- Several FT2 attributes are only skipped, not understood (410; 406/other kicker sounds; 703, 305, 306, 407 seen but unused).
- Group-1 wall fallback is a pattern seen in the data, not verified against the original exe's behaviour.
- Nothing has been run with a real display, audio, or input.

## Next steps
1. Run the build on a machine with a display; check the table draws, flippers move and the ball rolls (set FT2_DAT, put DAT next to binary).
2. Add a screenshot or frame-dump option so rendering can be checked headless.
3. Ghidra on one table exe (Alien is smallest) to recover its control logic; port it as a per-table control file.
4. Replace the FT2_DAT env hack with real table selection (menu or command line).

## Files
- ft2_decomp_experiment.patch - all decomp changes
- dat_dump.py - lists named groups and bitmap kinds in a .DAT
- alien_groups.txt / hero_groups.txt / mad_groups.txt - output of dat_dump.py


## 2026-11-02: Full Tilt 1 rule tables
Full Tilt 1 (1995) shipped 16-bit Windows 3.1 exes (NE format). Each table exe has a rule table in its data segment:
14-byte entries {far ptr to tag (-> far ptr to component name), far ptr to rule function, int16 score count, far ptr
to int32 scores}. Far pointers are filled by NE relocation chains, so the raw file holds no pointer values;
tools/ne.py resolves them. tools/ft1rules.py dumps the table.
Check: CADET.EXE gives 88 entries, the same as control.cpp's score_components[88], first entry a_bump1 {500,1000,1500,2000}.
So DRAGON.EXE (75) and PIRATES.EXE (74) are read the same way. All rule functions sit in code segment 23.

Also learned:
- Marble Drop (1997) uses the same PARTOUT(4.0) container and engine (Maxis South = Cinematronics).
- Some FT1 groups hold two bitmaps per resolution (Dragon/Pirates balls: real ball + smiley). Keep the first.
- Headless testing: under Xvfb the engine starts a game by itself; pressing F2 restarts it and the ball is served after
  the table's start sound (Skulduggery 1.67 s + 0.96 s feed). Wait ~3 s before pressing launch in scripts.
