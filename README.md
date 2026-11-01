# Full Tilt! 2 Pinball — native port (work in progress)

Goal: run the three **Full Tilt! 2 Pinball** tables (Alien, Hero, Mad) as one modern, native program,
built from readable source. No original table `.exe` is run or patched; the game reads only the
table data files (`ALIEN.DAT`, `HERO.DAT`, `MAD.DAT`).

Based on the [SpaceCadetPinball decompilation](https://github.com/k4zmu2a/SpaceCadetPinball) by
k4zmu2a and contributors (full history kept; their original README is in `README_SpaceCadetPinball.md`).

## Status
- All three tables load their components (physics, art, sounds) on the decomp engine.
- Not yet confirmed to draw correctly on screen. No table rules yet (missions, scoring, lights).
- Notes, format findings and DAT tools: [`ft2/`](ft2/), see `ft2/PROGRESS.md`.

## Plan
1. Playable build: table picker, working physics, generic scoring, ball count, game over.
2. Windows release built by GitHub Actions (64-bit, MSVC), no DLL shims or exe patching.
3. Rebuild each table's real rules from the original exes, one table at a time (Alien first).

## Running (current experimental build)
Put the table's `.DAT` next to the binary and set `FT2_DAT`, e.g. `FT2_DAT=ALIEN.DAT ./SpaceCadetPinball`.
The game data is not included; you need your own copy of Full Tilt! 2.

## Support
If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
