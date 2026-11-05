# 3-D Ultra Pinball (Sierra, 1995) - native

Sierra's 3-D Ultra Pinball (the Colony, Mine and Command Post levels) running natively on modern Windows and Linux:
no Windows 95, no Win32s, no emulator. The original 32-bit Windows program is **recompiled to C on your own
computer from your own copy of the game** and runs on a small native runtime (SDL2).

## Playing it

1. Copy your 3-D Ultra Pinball CD into `Put game files here`: the `UPINBALL` folder from the CD (or an installed
   `C:\SIERRA\UPINBALL`), or simply the CD image itself (`.iso`). Click **Scan for new game files**. A
   *3-D Ultra Pinball* card appears.
2. Click **Compile 3-D Ultra Pinball** at the bottom of the launcher (once; a minute or two). It needs Python 3, the
   `capstone` module and a C compiler, the same as Epic Pinball: **Install build tools** sets them up on Windows.
3. Click the card. The game starts with its own menus: click *Play Pinball*, pick the players and level in *New
   Game Options*, then *OK*.

Keys: the game's own (Space plunger, the Shift keys flip; see *How to Play* in the game), **Alt+Enter** fullscreen,
**F12** back to the table library. Settings and high scores the game writes go to `save/` in the card's folder;
your files are never changed.

From a source checkout: `cmake -DSIERRA_GAME_DIR=<your UPINBALL folder> ...` builds `UltraPinball` into `bin/`, or
run `python sierra/tools/build_sierra.py --game <folder> --out bin` directly.

## Why it is built locally

The recompiled C is a direct translation of the original program, so it is Sierra's code. This repository only
contains the tools and the runtime written for this project; the generated code (`sierra/gen-local/`) and the
`UltraPinball` program stay on the computer that built them, made from that person's own copy of the game. Please
do not commit or redistribute them.

## How it works

- `tools/sprecomp.py` - static recompiler for 32-bit Windows programs. Finds the reachable x86 code in the PE
  (recursive descent from the entry point, plus every relocated pointer into code: function pointers, vtables,
  jump tables) and turns each basic block into a C function over the runtime's CPU state, with exact x86 flag
  semantics and an x87 FPU. `--parts N` splits the output so it compiles in parallel. Game data is not copied: the
  runtime loads the original EXE image at run time and checks it matches.
- `tools/sres.py` - lists and extracts the game's resource archives (`RESOURCE.MAP` + `RESOURCE.00n`).
- `tools/build_sierra.py` - recompiles `PINBALL.EXE` and compiles everything with MSVC, MinGW or gcc/clang (shares
  the compiler detection with `epic/tools/build_epic.py`).
- `runtime/` - C99 runtime:
  - `rt_cpu.c` dispatcher, string instructions, x87 helpers, calls between native and recompiled code
  - `rt_mem.c` guest address space and heap, loading the EXE and binding its imports, file names (the game
    believes it is installed in `C:\SIERRA\UPINBALL`; any path is mapped to the game folder, writes go to `save/`)
  - `rt_win32.c` the Windows API the game uses: KERNEL32 (files, INI files, memory), USER32 (windows, child
    windows, the message queue, modal dialogs, mouse capture), GDI32 (8-bit DIB sections, palettes, BitBlt) and
    WINMM
  - `rt_sos.c` the HMI SOS sound library the game loads (`SOS9502.DLL`): device detection, samples and streams
    mixed on SDL2 audio, MIDI songs (accepted, not yet heard)
  - `rt_main.c` SDL2 window, keyboard and mouse, timing, test scripts
- The display is modelled as the 256-colour screen the game was made for: the window holds palette indices shown
  through the realized palette, so the game's palette fades work as they did.

## Status

- Plays: Sierra logo, main menu, New Game Options, all three levels; sound effects and speech.
- Music: not yet. The songs are HMI MIDI files written for an FM (OPL) card or General MIDI; a synthesizer for them
  is next.
- Intro and finale videos (`SHELL\*.AVI`) are skipped. The help file viewer (`PBHELP.EXE`) is not included.

## Debugging

`SIERRA_TRACE=1` copies the log (`save/sierra.log`) to stderr; `SIERRA_TRACE_GDI`, `SIERRA_TRACE_INPUT`,
`SIERRA_TRACE_SOUND` and `SIERRA_TRACE_CALLS` (every Windows call with its arguments) add detail. When the game
reaches code that was not recompiled, the address is written to `save/missing_seeds.txt`: add it to
`tools/seeds/PINBALL.txt` and rebuild. `--script FILE --headless` plays timed keys and clicks and saves screenshots
(see `runtime/rt_main.c`).
