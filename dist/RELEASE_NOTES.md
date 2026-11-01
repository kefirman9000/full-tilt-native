**Full Tilt Native**: Full Tilt! Pinball tables running natively on Windows 10/11 (64-bit), with no original game programs involved.

**What's in it**
- `FullTiltPinball.exe` — table library launcher (a card per table)
- `FullTiltEngine.exe` — the game engine (built on the SpaceCadetPinball decompilation)

**Game data is not included.** Copy your original game folders into `Put game files here` and start `FullTiltPinball.exe`: tables are found and sorted automatically.

**New in this version**
- Fix: tables picked in the launcher failed with "The .dat file is missing" on Windows
- Music: Full Tilt 2 and Full Tilt 1 tables now play their own MIDI music
- CD audio: every table folder has a `Music` folder. Put the table's CD tracks there (mp3/ogg/wav/flac, played in name
  order: 01.mp3, 02.mp3 ...) and they replace the MIDI tracks. Tracks without a file keep the MIDI music.
- Starting FullTiltEngine.exe by itself opens the table library instead of an error

**Status**
- Alien Daze, Mad Scientist: playable with simple placeholder rules (points, 3 balls, game over)
- Space Cadet: full original rules
- Captain Hero, Dragon's Keep, Skulduggery: work in progress

If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
