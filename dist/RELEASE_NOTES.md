**Full Tilt Native**: Full Tilt! Pinball tables running natively on Windows 10/11 (64-bit), with no original game programs involved.

**What's in it**
- `FullTiltPinball.exe` — table library launcher (a card per table)
- `FullTiltEngine.exe` — the game engine (built on the SpaceCadetPinball decompilation)

**Game data is not included.** Copy your original game folders into `Put game files here` and start `FullTiltPinball.exe`: tables are found and sorted automatically.

**Fixes in 0.8.1 / 0.8.2**
- **Mad Scientist's plunger tube only bubbles while you pull the plunger back**, not all the time.
- **Pockets spit the ball out properly** (Mad Scientist, and pockets on every table that had the same problem): many
  kickouts and sinks describe their eject as a line from the pocket to where the ball should go. That line was
  ignored, so the ball was thrown with no direction. It is now the throw direction.
- **Captain Hero's mid-table magnet pockets no longer grab the ball just from rolling over them.** The original rules
  only switch them on during modes; they now stay closed and open for 20 seconds when you complete a set or bank.

**New in this version**
Every table that doesn't have its original rules yet (all but Space Cadet) gets real pinball goals, built from its
own parts (Options > Table Rules > Enhanced Rules, on by default):
- **Drop target banks**: knock down a whole bank for a bonus, then the bank pops back up.
- **Lane and target sets**: hit every rollover lane / standup in a set for a bonus; the lane lights reset.
- **Bonus multiplier**: each completed set or bank raises it, up to x5.
- **End-of-ball bonus**: every scoring hit adds to the bonus, paid out times the multiplier when the ball drains.
- **Ball save**: lose the ball within 8 seconds of launching and you get it back once (Off / 5 / 8 / 15 seconds).
- **Extra ball** at 1,500,000 points (once per player per game), with "Shoot Again".
- **Light shows** on completions and extra balls, and an attract light show after the game ends.
- Messages on the table for all of these.


**Graphics** (Options > Graphics):
- **Glow**: lit lights and bright parts bloom softly (Off / Soft / Strong; Soft is on by default).
- **Brightness**: Normal / Bright / Brighter, for dark screens and dark tables.
- **Vignette**: slightly darker edges that pull the eye to the playfield (on by default).
- **Motion Trails**: the ball and flippers leave a short streak (off by default).

**Launcher**:
- **Report a problem** button: collects the logs and settings (never game files) into FullTiltReport.zip next to
  the programs and shows it in Explorer, ready to attach to a GitHub issue.

Also: Skulduggery's ball no longer falls out of the table, and the Full Tilt 2 tables play their sounds (0.7.1).

**Status**
- Alien Daze, Captain Hero, Mad Scientist, Dragon's Keep, Skulduggery: playable with simple placeholder rules
- Space Cadet: full original rules

If you enjoy this project, you can support it through [GitHub Sponsors](https://github.com/sponsors/kefirman9000).
