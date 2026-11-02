**Full Tilt Native**: Full Tilt! Pinball tables running natively on Windows 10/11 (64-bit), with no original game programs involved.

**What's in it**
- `FullTiltPinball.exe` — table library launcher (a card per table)
- `FullTiltEngine.exe` — the game engine (built on the SpaceCadetPinball decompilation)

**Game data is not included.** Copy your original game folders into `Put game files here` and start `FullTiltPinball.exe`: tables are found and sorted automatically.

**New in this version**
- **Sound effects on the Full Tilt 2 tables**: they were silent. Their effects live in Sound\high (and Sound\lower),
  which the engine never searched. Alien Daze, Captain Hero and Mad Scientist now have all their sounds.
- **Alien Daze uses its real point values**, recovered from the original Alien.exe rule table (bumpers 1,500-7,500,
  drop targets 5,000-50,000, ramps 25,000-500,000...). First step of rebuilding its original rules.
- **Full Tilt 1 parts**: Skulduggery's accelerators and ramp exits, Dragon's Keep's diverters now exist on the table
- **Smooth Flippers**: in-between flipper positions at high frame rates (Options > Graphics)
- **Side Score Panel** (Options > Graphics, Full Tilt 2 tables): Full Tilt 1-style column beside the table with the
  table's art (preview.bmp), player, score, ball and messages
- From v0.6.0: animations (Mad's bubbling), sharp scaling, backdrop, light fades, scanlines

**Status**
- Alien Daze, Captain Hero, Mad Scientist, Dragon's Keep, Skulduggery: playable with simple placeholder rules
- Space Cadet: full original rules

If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
