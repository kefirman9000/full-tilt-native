# Epic Pinball (1993) - native

Epic Pinball's 13 tables running natively: no DOSBox, no emulator. The original 16-bit programs are **recompiled to
C on your own computer from your own copy of the game**, and run on a small native runtime (SDL2).

- **Whole table on screen.** The default view shows the entire 320x400 playfield plus the score panel, so the table
  never scrolls. **F11** switches to the original 1993 scrolling window (and back) at any time.
- **No DOS setup screens.** The sound-card setup is done automatically (it writes the same files the original SETUP
  program would, into a separate `save` folder).
- **Your files are never changed.** Settings and high scores the game writes go to `save/` inside the table folder.

## Playing it

1. Copy your Epic Pinball folder (with `PINBALL.EXE`, `EP1.EXE` ... `EP13.EXE`, the `.DAT` files) into
   `Put game files here`, then click **Scan for new game files**. An *Epic Pinball* card appears.
2. Click **Compile Epic Pinball tables** at the bottom of the launcher (once). It takes a few minutes and needs:
   - Python 3 (python.org; the `capstone` module is installed automatically), and
   - a C compiler: *Build Tools for Visual Studio* with the C++ workload, or MinGW-w64.
3. Click the *Epic Pinball* card. Pick a table in the game's own menu.

Keys: the game's own (Z and / or the Shift keys flip, Space launches; see each table's rules), **F11** whole table /
original scrolling view, **Alt+Enter** fullscreen, **F12** back to the table library.

From a source checkout: `cmake -DEPIC_GAME_DIR=<your Epic Pinball folder> ...` builds `EpicPinball` into `bin/`, or run
`python epic/tools/build_epic.py --game <folder> --out bin` directly.

## Why it is built locally

The recompiled C is a direct translation of the original programs, so it is Epic MegaGames' code. This repository
only contains the tools and the runtime written for this project; the generated code (`epic/gen-local/`) and the
`EpicPinball` program stay on the computer that built them, made from that person's own copy of the game. Please do
not commit or redistribute them.

## How it works

- `tools/eprecomp.py` - static recompiler. Finds all reachable x86 code in an MZ executable (recursive descent, jump
  tables, interrupt handlers stored by address, far/near function pointer tables, functions that follow other
  functions) and turns every basic block into a C function over the runtime's CPU state, with exact x86 flag
  semantics. Game data is not copied: the runtime loads the original EXE image at run time.
  MASI sound driver modules (`MDRVnnnR.MUS`) are recompiled too; their immediates are read from memory, because
  the game and its setup patch settings into the driver code.
- `tools/seeds/*.txt` - extra entry points (addresses only) found by running the game, for code reached only through
  computed pointers.
- `runtime/` - C99 runtime: dispatcher, interrupts, PIC/PIT timer, keyboard controller (`rt_core.c`), the DOS and
  BIOS services the game uses (`rt_dos.c`), VGA with planar "Mode X" memory, text mode and the two views
  (`rt_vga.c`), SDL2 window, keyboard and test automation (`rt_main.c`). `rt_font.c` is built from the public-domain
  font8x8 by Daniel Hepper.
- `tools/build_epic.py` - runs the recompiler on all 21 programs and compiles everything with MSVC, MinGW or gcc/clang.

The game keeps the *entire* table in video memory (two 400-row pages; the 20-row score panel sits at the start of
video memory and is shown through the VGA split screen). The original view renders what the 1993 CRTC registers
describe (a scan-doubled 320x240 window moved by the start address); the full view draws all 400 rows of the
displayed page with the score panel below, with square pixels. No game code is changed for either.

## Status

- All 13 tables start, play and switch views (tested with scripted play on Linux).
- Sound: not yet - the game runs with its "no sound card" driver. Next: a native Sound Blaster so the original music
  and effects play through the game's own driver.
- Settings window (view, sound, fullscreen) in the launcher style: planned.

Debug aids: `EPIC_TRACE_FILES=1`, `EPIC_TRACE_DOS=1`, `EPIC_TRACE_BIOS=1` log DOS/BIOS activity; when the game reaches
code that was not recompiled, the address is written to `missing_seeds.txt` (add it to `tools/seeds/<PROGRAM>.txt`).
`--script FILE` plays timed key presses and saves screenshots (see `runtime/rt_main.c`).
