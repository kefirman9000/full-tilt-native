**Full Tilt Native**: Full Tilt! Pinball tables running natively on Windows 10/11 (64-bit), with no original game programs involved.

**What's in it**
- `FullTiltPinball.exe` — table library launcher (a card per table)
- `FullTiltEngine.exe` — the game engine (built on the SpaceCadetPinball decompilation)

**Game data is not included.** Copy your original game folders into `Put game files here` and start `FullTiltPinball.exe`: tables are found and sorted automatically.

**New in this version**
- **Skulduggery: the ball no longer falls out of the table.** The ramp exit lines added in 0.7.0 were on every layer,
  including the main playfield, so a ball rolling over one was moved onto a ramp layer with no floor under it.
  They now only exist on the ramp they belong to.
- **Full Tilt 2 tables play their sounds.** These tables have almost no sounds attached to parts: the original rules
  play them. The sound each part's rule plays was recovered from Alien.exe, hero.exe and mad.exe (107 parts across
  the three tables: bumpers, targets, kickouts, ramps, plunger...) and now plays on hits.
- Sound files are found whatever their folder/file case (Sound\high, SOUND, tabe01.wav, TABE02.WAV), and any sound
  that can't be found is written to FullTiltEngine.log.
- If you still hear nothing: check Options > Sounds is ticked (F-key toggles can turn it off).

**Status**
- Alien Daze, Captain Hero, Mad Scientist, Dragon's Keep, Skulduggery: playable with simple placeholder rules
- Space Cadet: full original rules

If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
