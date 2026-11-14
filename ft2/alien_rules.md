# Alien Daze rules (recovered from Alien.exe)

Source path in the exe: `alien\src\control.cpp`. Rule table: 63 entries of 16 bytes at 0x442dd8
`{tag*, rule function, score count, scores*}` (listing: `ft2/alien_controls.txt`). Tags are
`{name*, component*, ...}`, so a reference to `tag+4` is that part. The port is `SpaceCadetPinball/ft2_alien.cpp`;
its comments give the original function for each rule.

How it was read: Ghidra's C loses most call arguments (they are pushed on the stack before indirect calls), so
the disassembly was annotated with part names, text ids and helper names and the pushes folded back into calls
(`ft2/tools/annot.py`, `ft2/tools/calls.py`; texts from the exe's string table with `ft2/tools/pestrings.py`).

## Engine conventions in Alien.exe
- Rule messages: 0x4c collision, 0x58 ramp pass, 0x4b spinner, 0x56 ramp entry, 0x48 kickback, 0x3f blocker
  timer, 0x3f9 reset, 0x3f3 tilt, 0x402 bank reset. Table events reach the plunger rule (0x4300ac): 1004-1019.
- Light messages keep Space Cadet's light numbers (4 = flash), plus 27 = on, 28 = off. Groups: 32/33 chase
  forward/backward with a delay, 40/41 stop/start.
- Helpers: 0x42c768 add score x multiplier; 0x42c7c8 jackpot += points x multiplier (cap 10,000,000, shown in
  box 3); 0x401434(box, text, seconds) / 0x401314(box, string, seconds) display; 0x4292b4 timer, 0x4294ac kill.
  Boxes: 4 messages, 5 mission, 6 mission countdown, 8 extra ball, 9 multiplier.
- Table fields: +0x141 jackpot (1,000,000 at the start of a game), +0x161 balls on the table, +0x155 balls left,
  +0x15d extra ball, +0x181 tilt, +0xd1 players, +0xd5 current player.

## Missions (0x42cd20 choose, 0x42d16c start, 0x42ceb0 stop, 0x42cfb4 completed)
- The next mission is picked at random from the ones not done yet; Re-assemble Abductee is last, once the other
  four are done; after all five they start over. Its mode light flashes, box 5 shows "Next Mission: ...".
- The landing ramp (s_ramp15) picks another one while none is running. The cornfield center hole (corn_sink_2)
  starts it, in single-ball play.
- Time limits (0x443540): 30, 25, 35, 25 s, none for the last. A timed mission is completed when its time is up.
- s_sink02 during a mission: "Extend Mission (right flipper) or Cancel Mission (left flipper)", 10 s; extend
  adds 15 s (0x431f84).
1. **Make Crop Circles** (0x430510): corn_sink_3 small 50k-150k, corn_sink_2 medium 200k-800k, corn_sink_1 large
   100k-500k (ladders 0x441b78 / 0x441b48 / 0x441b18), each x multiplier, "... Crop Circle: %ld points".
2. **Abduct Humans** (0x42df88, 0x42e088): 17 targets (0x4439c4, with their lights). The first hit on each names an
   abductee (texts 3132 + round*17 + target) and scores 20,000 + 2,000 per abductee so far (max 100,000). All 17:
   a new round with a new crowd.
3. **Re-capture Bigfoot** (0x42f62c, 0x42f498, 0x42f58c, 0x42f74c): Bigfoot stands at one of a_bigfoot01-06 for
   5 s, then walks to the next spot, lighting the footprints Path(n)Print01.. one by one (1.5 s / (prints + 1)
   per print; path lengths 14, 6, 8, 4, 3, 1). Hitting him: 100,000, and he appears somewhere else.
4. **Take a Joy Ride** (0x42d560, 0x42e88c, 0x43079c, 0x431570): ship lights chase. The space alien bank: "Play
   Tag with ..." 1,000,000 (9 vehicles); each target 25,000. corn_sink_3: "Buzz ..." 50,000 (9 places). Loop:
   tabloid appearance 50,000 (9). Landing ramp 25,000.
5. **Re-assemble Abductee** (0x42e238, 0x42e3b8, 0x42e510): the UFO targets a_drop09-14 are Legs, Arms, Head,
   Torso, Heart, Brain (100,000 each); the space alien targets are a Horse Head, Chicken Legs and a Cow Brain
   (50,000) that take the place of the head, legs and brain. s_sink02: "Tracking Device" 250,000. Complete: 400,000
   (200,000 with a wrong part), then the landing ramp releases the abductee or the loop keeps it ("Eat for Lunch"):
   jackpot + 500,000, and the jackpot starts again at 1,000,000.

## Spacecraft
- Gear targets a_spot04/03/02/01 light v_lite74/73/75/76 ("Gear Lowered"), 7,500 (15,000 after landing).
- Landing (0x430de4): the landing ramp lands the ship, scored by the gear down: 0-4 gear "Emergency Landing --
  Cow Crushed" ... "Perfect Landing -- Cow Missed" (texts 3113-3117, waves 33-37), s_ramp15's scores
  25k/50k/100k/250k/500k.
- After landing (0x431088): each landing ramp pass climbs v_lite72/71/70: 75k, 150k, 300k, 600k; a step back every 30 s.
- All four gear down after landing: "Spacecraft Lock Enabled" (v_lite83). s_sink01 then locks the ball (0x4318cc,
  sockets socket01-04 show it); the fourth lock starts multiball out of ship_exit_sink.
- s_sink01 otherwise (0x431ad8): 50,000; every third shot raises the bumper value (stages 1,500 .. 7,500).

## Other rules
- Bumpers (0x42dd74): stage score, jackpot +2,000. In single-ball play without a mission, the 11th hit shows
  "Bumpers Activate Tractor Beam" and the 12th switches it on (s_kout01 open, a_cover off, v_lite80 on).
- Tractor beam (0x42dcac, 0x42da44): the ball floats along the beam up into the spacecraft (encasement) and a new
  ball is served; 100,000 more per lift, the 7th 1,000,000 and jackpot +1,000,000.
- Space alien targets a_drop05-07 (0x42e964): all three light the stun magnet (v_lite84). The magnet s_mag01 (type
  1042, a circle of radius 13.35 around the stun kickout, pull 2000) catches the ball: 250,000, "Stun Magnet
  Active"; aim lights v_lite85-92 go round and a flipper press (or 15 s) fires the ball at the lit one (0x42ff6c).
- Recharge targets a_drop01/02 (0x42eb8c): both down add a reserve power charge (v_lite64/63/62, up to three) for
  the outlane kickback power_reserve_intake / _outlet.
- UFO targets a_drop09-14 (0x42efc8): 5k/10k/15k/20k/30k, the bank 50,000 and the multiplier up to 3 ("2X", "3X").
- Cornfield targets a_spot05/06 (0x42fb8c): both light the extra ball at the cornfield (v_lite65, once a game).
- Spiral ramp s_ramp01 (0x430c48): v_lite69/68/67; "Once More to Hold Bonus", "Bonus Held"; 50k-400k; back one step
  every 25 s. Loop s_ramp13 (0x43147c): v_lite79/78/77, the third lights the extra ball; 40k-320k; 20 s.
- Artifact trade at s_sink02 without a mission (0x431c24, 0x431e3c): "Trade Alien Artifact for %s?", 80 artifacts
  (texts 3008-3087); worth 50,000 + 1,000 x n (10,000 + for n > 39), the right answer pays it in full, the other half.
- Spinner 1,000 and jackpot +15,000 per spin; slings 350; cow 25,000; lanes 10k/25k/10k/5k/5k.
- End of ball (0x432110): missions x 200,000, ship landed 100,000, abductees x 20,000, artifacts x 10,000,
  encasements x 100,000, plus a held bonus; "Total Bonus". The multiplier goes back to 1.

## Not ported yet
- The skill shot (plunger rule 0x4300ac, 1004-1019; skill shot light v_lite81), cornfield multiball (0x4309a4 on
  reset: "[ Multiball" / "[ 250,000" / "[ 75,000" at the cornfield holes), the levitation path animation
  (0x4435d8), the gate behind the ray gun (s_onew01 raises a_gate01 for 2 s), music changes per mission
  (tabe1-8 MIDI), the loop diverter (0x431608).
