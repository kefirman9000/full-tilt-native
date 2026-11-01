# Full Tilt Native

Full Tilt! Pinball (Full Tilt 1 and 2) tables running natively on modern Windows: one launcher, one engine, no original exes.

Goal: run the **Full Tilt! 2 Pinball** tables (Alien Daze, Captain Hero, Mad Scientist) and Full Tilt 1's tables as one modern, native program,
built from readable source. No original table `.exe` is run or patched; the game reads only the
table data files (`ALIEN.DAT`, `HERO.DAT`, `MAD.DAT`).

Based on the [SpaceCadetPinball decompilation](https://github.com/k4zmu2a/SpaceCadetPinball) by
k4zmu2a and contributors (full history kept; their original README is in `README_SpaceCadetPinball.md`).

## Status
- Alien and Mad are playable with generic rules (launch, scoring, drain, next ball, game over). Hero partly.
- Full Tilt 1's Dragon's Keep and Skulduggery load too (work in progress).
- No real table rules yet (missions, lights, modes). Notes: `ft2/PROGRESS.md`, plan: `ft2/ROADMAP.md`.

## Plan
1. Playable build: table picker, working physics, generic scoring, ball count, game over.
2. Windows release built by GitHub Actions (64-bit, MSVC), no DLL shims or exe patching.
3. Rebuild each table's real rules from the original exes, one table at a time (Alien first).

## Running
Easiest: download the latest release, then copy each table's `.DAT` and sound folder from your own copy of the games into
`tables/<Table>/` (see the release README) and start `FullTiltPinball.exe` (the table library launcher).

By hand: put the table's `.DAT` in the working folder and run `FullTiltEngine --table ALIEN.DAT`
(or HERO.DAT, MAD.DAT, Full Tilt 1's DRAGON.DAT / PIRATES.DAT). Without `--table`, CADET.DAT plays Space Cadet with
its full original rules. Game data is not included; you need your own copy of the games.

## Support
If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
