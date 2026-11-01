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
- [ ] GitHub Actions: build Windows release with MSVC on every tag; in-app updater (asks before updating)

## Next
- [ ] Optional **Full Tilt 1-style side panel** for FT2 tables (score, ball, player, messages beside the table).
      Panel art: the FT2 title screens the user supplied (Alien Daze, Mad Scientist, Captain Hero), with the stray
      hourglass cursor painted out; table icons (Alien.bmp / Hero.bmp / Mad.bmp) as fallback. Art is NOT committed
      (copyrighted game art); it ships with the user's local package.
- [ ] Remove debug hooks (FT2_DEBUG, FT2_NOZ) or keep behind a developer option

## Later: real rules, table by table
- [ ] Ghidra on Alien.exe (smallest). Use Full Tilt 1 CADET.EXE as a reference: its code is what the decomp was
      written from, so matching functions mark shared engine code, and what is left in Alien.exe is the table rules.
- [ ] Port each table's rules into its own control file (like control.cpp for Space Cadet)
- [ ] Same for Hero, Mad, then Dragon's Keep and Skulduggery

## Long term
- [ ] A **library of retro decompiled pinball games**: one launcher, many tables/games, each with its own data
      folder and rules module. Keep table-specific code isolated (per-table control files, per-table flags) so new
      games can be added without touching the others.
