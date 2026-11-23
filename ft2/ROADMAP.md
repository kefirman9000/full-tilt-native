# Roadmap

## Now (sandbox builds)
- [x] Alien, Hero, Mad load and draw on the decomp engine (no original exe involved)
- [x] Generic rules (ft2_control): points per component type, kickouts/sinks re-fire, drain serves next ball, game over
- [x] Full Tilt 1 extra tables (Dragon's Keep, Skulduggery) load with the Space Cadet layout + generic rules
- [x] `--table NAME.DAT` command line option; Windows 64-bit build (llvm-mingw, UCRT, SDL2)
- [x] Hero: plunger shot goes up the wire ramp and comes back down: only a too-soft launch does that now (it rolls
      back to the plunger, as on the original); a short pull is the skill shot, a harder one plays normally
- [x] Dragon's Keep launches; Skulduggery loads and plays (launcher 1034 as plunger, blocker a_blocf raised)
- [ ] Unknown FT engine object types: 1034-1038, 1042-1045 (skipped today)
- [ ] Table picker in the game (menu), instead of launch scripts
- [x] GitHub Actions release build (MSVC) + in-app updater (asks before updating) + log files

## Next
- [x] Optional **Full Tilt 1-style side panel** for FT2 tables (0.9.0: on by default, dot-matrix display, rule status;
      art = the SPLASH_BITMAP title screen read from the table's own exe at run time, see table_art.cpp) (score, ball, player, messages beside the table).
      Was planned: the FT2 title screens the user supplied (Alien Daze, Mad Scientist, Captain Hero), with the stray
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
- [x] Dragon's Keep: every rule function and quest handler of DRAGON.EXE segment 23 translated
      (SpaceCadetPinball/ft1_dragon.cpp, on by default; FT1_DRAGON_OLD=1 for the earlier rules);
      engine parts added for it: FT1 blockers, light sweeps (1038), holes that keep the ball, ramp events 84-86.
- [ ] Dragon's Keep: the info box's own messages (0115) and timed texts (0000/00db), the picture font.
- [ ] Skulduggery (PIRATES.EXE, 179 functions in segment 23) translated the same way.
- [ ] Skulduggery's cannon (a_cannon, 4-frame blocker) and launcher have no rules yet; launcher has no art in the data.

## Status 2026-11-03 (Captain Hero rules, first pass)
- [x] Hero rule table recovered from hero.exe (133 entries; ft2/tools/ft2rules.py, notes in ft2/hero_controls.txt).
      Hero scores straight from its rule functions through one helper (0x401a38), so the tool also lists the points
      each function passes to it. Mad Scientist's table reads the same way (53 entries in mad.exe).
- [x] Real Hero point values in use (ft2_scores.h); pops escalate 1,000..5,000; henchmen take 5 hits (10,000 x hit)
- [x] Hero skill shot (ss_trigger): soft launch that only just reaches the top, 500,000 / 1,000,000 / extra ball
- [x] Hero battles (0x43d088 1-6 = Quake, Mad Scientist, Mystman, Slime Man, Mr. Pling, Evil Twin; 7 = "Battle
      Villain"): offered at s_sink01 (0x406580, flippers choose: 0x406500/0x406528), started by 0x40787c,
      won via 0x407e08(1) (jackpot, reset to 500,000), lost on drain. Table texts from hero.exe's string table.
- [x] Hero: battle timers, Evil Twin's Krypton-@ rule and Super Jackpot, mystery award, buildings (0.12.0)
- [x] Hero: Mr. Pling's second ball out of s_sink13 (stage 7 is only a debug mode)
- [x] Hero ball saver s_sink30 shut except ball save time / forcefields (was the "endless ball")
- [x] Hero Jackpot (+0x141) and Super Jackpot (+0x149) growth and multiplier
- [x] Hero pop multiplier (0x405de8) and Super Strength/Speed/Flight (0x4061f0)
- [x] Mad Scientist: rule table + scores (points in data variables; ft2rules.py reads their starting values)

## Status 2026-11-13 (0.10.0: Alien Daze rules)
- [x] Alien Daze's rules ported from Alien.exe (SpaceCadetPinball/ft2_alien.cpp, notes in ft2/alien_rules.md):
      five missions, landing gear / landing / spacecraft lock and multiball, tractor beam, stun magnet (s_mag01,
      type 1042, done as a pull circle around the stun kickout), reserve power, multiplier, bonus hold, extra
      ball, artifact trading, jackpot, end-of-ball bonus. Texts come from the user's Alien.exe at run time.
- [x] Alien: cornfield skill shot (with its multiball) and the shoot again light
- [x] Alien and Mad: music per mission / experiment (tabe2-6, tabd2-6)
- [ ] Alien: the levitation path animation, a_gate01 behind the ray gun, mission
      music, loop diverter
- [x] Mad Scientist rules (ft2/mad_rules.md, SpaceCadetPinball/ft2_mad.cpp)

## Status 2026-11-15 (0.12.0: Captain Hero rules)
- [x] Captain Hero's rules from hero.exe (ft2/hero_rules.md): timed battles, Evil Twin + Super Jackpot, jackpots,
      mystery award, buildings, Be A Hero, extra ball, bumper boost, ramp combo, end-of-ball bonus.
- Decompiling without Ghidra: angr's decompiler (pip install angr) on the control code range gives C with the
  original asserts; label it with part names, texts and helpers (see hero_rules.md).
- Testing rules without aiming: FT2_DEBUG=1 FT2_HITS="part,part,..." sends one hit every 0.6 s after New Game
  ("wait", "drain", flippers allowed); every message the rules show is logged as "FT2DBG show:".
- [ ] Next: Dragon's Keep and Skulduggery rule ports (ft1lift/ft1draft), per-table self-test scenarios in CI.
