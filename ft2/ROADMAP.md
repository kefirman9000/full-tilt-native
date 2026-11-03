# Roadmap

## Now (sandbox builds)
- [x] Alien, Hero, Mad load and draw on the decomp engine (no original exe involved)
- [x] Generic rules (ft2_control): points per component type, kickouts/sinks re-fire, drain serves next ball, game over
- [x] Full Tilt 1 extra tables (Dragon's Keep, Skulduggery) load with the Space Cadet layout + generic rules
- [x] `--table NAME.DAT` command line option; Windows 64-bit build (llvm-mingw, UCRT, SDL2)
- [ ] Hero: plunger shot goes up the wire ramp and comes back down (ramp exit / gate needs rules)
- [ ] Dragon's Keep: ball not launching yet; Skulduggery: hangs on load (no plunger, unknown object types 1034-1038, 1042)
- [ ] Unknown FT engine object types: 1034-1038, 1042-1045 (skipped today)
- [ ] Table picker in the game (menu), instead of launch scripts
- [x] GitHub Actions release build (MSVC) + in-app updater (asks before updating) + log files

## Next
- [ ] Optional **Full Tilt 1-style side panel** for FT2 tables (score, ball, player, messages beside the table).
      Panel art: the FT2 title screens the user supplied (Alien Daze, Mad Scientist, Captain Hero), with the stray
      hourglass cursor painted out; table icons (Alien.bmp / Hero.bmp / Mad.bmp) as fallback. Art is NOT committed
      (copyrighted game art); it ships with the user's local package.
- [x] Debug hooks kept as developer options, read once at start-up (pb::DevDebug = FT2_DEBUG, pb::DevNoZ = FT2_NOZ)

## Later: real rules, table by table
- [ ] Ghidra on Alien.exe (smallest). Use Full Tilt 1 CADET.EXE as a reference: its code is what the decomp was
      written from, so matching functions mark shared engine code, and what is left in Alien.exe is the table rules.
- [ ] Port each table's rules into its own control file (like control.cpp for Space Cadet)
- [ ] Same for Hero, Mad, then Dragon's Keep and Skulduggery

## Long term
- [ ] A **library of retro decompiled pinball games**: one launcher, many tables/games, each with its own data
      folder and rules module. Keep table-specific code isolated (per-table control files, per-table flags) so new
      games can be added without touching the others.

## Status 2026-11-01 (v0.7.0)
- [x] All six Full Tilt tables play (simple rules); Space Cadet full rules
- [x] FT2 sound effects (Sound/high), table MIDI + Music folders for CD audio
- [x] Graphics: animations (1044/1045), sharp scaling, backdrop, light fades, smooth flippers, scanlines, side score panel
- [x] Launcher: library, Put game files here, high scores, updater (FullTiltUpdate.exe), one-window flow
- [~] Alien Daze real rules: rule table found in Alien.exe (63 components: name, rule function, scores);
      real point values in use (SpaceCadetPinball/ft2_scores.h). Next: port rule functions one by one
      (decompiled C via Ghidra; source path in exe: alien_src\control.cpp). Hero/Mad keep scores inside rule functions.
- [ ] Magnets (1042) and light sweeps (1038) need rules; Addiction Pinball needs its own engine

## Status 2026-11-02 (v0.8.3)
- [x] Full Tilt 1 rule tables recovered from the 16-bit NE exes (ft2/tools/ft1rules.py): CADET.EXE 88 entries (matches
      control.cpp exactly), DRAGON.EXE 75, PIRATES.EXE 74 (ft2/*_rules.txt). Real scores in use for both tables.
- [x] Dragon's Keep / Skulduggery: real ball instead of the hidden smiley (second bitmap per resolution kept as alternate)
- [x] Glow limited to lights; music on its own thread (no stall on toggle); upstream timer::kill(callback) bug fixed
- [x] Marble Drop marbles as ball styles (tables/_MarbleDrop, imported by the launcher)
- [ ] Port FT1 rule functions: all of Dragon's and Pirates' rules are in code segment 23 of their exes (function
      offsets in the dumps). Next: disassemble seg 23 (16-bit x86), starting with the launcher (s_launcher1 23:4152,
      lights v_lite569) and drain, then lights per target so tables light up during play.
- [ ] Skulduggery's cannon (a_cannon, 4-frame blocker) and launcher have no rules yet; launcher has no art in the data.
