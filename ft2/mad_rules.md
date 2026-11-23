# Mad Scientist rules (recovered from mad.exe)

Rule table: 53 entries `{tag*, rule function, score count, scores*}`; the score values live in globals at
0x444f84-0x445344 (initialised data). Read the same way as Alien Daze (`ft2/alien_rules.md`): Ghidra C plus the
disassembly annotated with part names, text ids and helper names (`ft2/tools/annot.py`, `ft2/tools/calls.py`).
The port is `SpaceCadetPinball/ft2_mad.cpp`; its comments give the original function for each rule.

## Engine conventions in mad.exe
- Helpers: 0x42d41c add score (also tells the previous part 0x4f), 0x42d4b4 part busy, 0x4293a4 timer,
  0x42959c kill timer, 0x401874 show text (box, text id, ..., seconds).
- Lights: 17 with 1/0 = on/off, 28 = off, 4/7 = flash, 9 = on for a time. Drop targets: 56 raise, 55 drop.
- Per-player block at 0x44a984: flask (+4..+0x10), mob done (+0x14), graveyard done (+0x18), six monster parts
  (+0x1c..+0x30), brain IQ (+0x44), brain place and index, elixir done (+0x54), jackpot (+0x58, 1,000,000 at the
  start), held bonus (+0x5c), current experiment (+0x60).

## Experiments (0x430e1c choose, 0x4304c4 start, 0x430a30 end, 0x430d38 timer)
- The next one is picked at random from those not done: Raid Graveyard for Parts, Fight Angry Mob!, Steal Brain
  for Monster, Make Elixir of Life. Bring Monster to Life comes once the parts, a brain and the elixir are in hand
  (the mob is not needed). Mode lights: graveyard mode_light04, mob 05, brain 03, elixir 02, monster 01.
- Igor (igor_sink, 0x42edc8) starts it in single-ball play: 150,000. During one, Igor adds 10 seconds
  ("Extend Experiment"). Time limit 40 s; none for Bring Monster to Life.
1. **Raid Graveyard**: the element D kickout gives Head, Left Arm, Left Leg; the element C ramp gives Torso, Right
   Arm, Right Leg (texts 3100-3105, body lights body03a/01a/04a/06a/02a/05a). All six: done.
2. **Fight Angry Mob** (0x42e89c, 0x42eb64): the nine angrymob drops come up (the monster targets go down); each
   villager 50,000 + 10,000 per one so far; five is a success, nine ends it.
3. **Steal Brain** (0x431570): Farm (mercury ramp), Morgue (element B ramp), Joe's Brain and Lube (element C
   ramp), Research Facility (tower sink), four brains each (Chicken 5 IQ ... Einstein 200 IQ, texts 3200-3215,
   IQ at 0x4451c4). A shot takes the place's next brain when it is better than the one held: 2,000 per IQ point.
4. **Make Elixir of Life**: elements into the flask are worth 100k/150k/200k/250k; with all four, the flask
   kickout mixes the elixir: 500,000.
5. **Bring Monster to Life** (0x42e0bc, 0x42e0ec, 0x42f118): the seven monster targets 100,000 each; all seven
   light the tower sink ("Bring Monster to Life"), which pays the jackpot (back to 1,000,000). Twenty seconds later
   the experiment ends and everything starts over.

## The flask (0x431ca4, 0x42fcb0, 0x4324bc, 0x431e60)
- Einsteinium (element A kickout), Lecithin (element B ramp), Yttrium (element C ramp), Bat Tongue (element D
  kickout), lights flask_01-04.
- The flask kickout with something in it asks "Mix Elements (Left Flipper) or Empty Flask (Right Flipper)?" for
  6 s. Empty: 50,000 per element, the ball comes back. Mix: the ball goes into the flask (flask_sink) and a new
  one is served; the four bits pick a potion (0x4452e4): points (10k..500k ladder), jackpot +500,000, teleporter,
  bumpers increased, shoot again, cloning light, max bumpers, plasma field, table multiplier (up to 5X), 500K,
  extra ball light, 1 million, multi-ball, 5X, extra ball (texts 3021-3035).

## Other rules
- Monster targets (0x42e0ec): 15,000; all seven 15k/30k/45k/60k and the teleporter lights; they come back up.
- Teleporter (0x42efc0): 150,000, "Ball Teleported", lights the cloning light.
- Mercury input sink (0x4328dc, 0x432774): 25,000, a combo within 10 s 50k-200k; with the cloning light, a clone
  ball comes out of the mercury tubes.
- Element B ramp 50,000 (combo 75k-300k), element C ramp 40,000 (combo 80k-200k), combo lights on for 10 s.
- Plasma targets (0x42e764): 25,000, both 150,000 and the plasma field: the B ramp, C ramp and mercury ramp in
  turn 250,000 each.
- Tower sink otherwise: Lightning Rod 50,000. Element A kickout: the extra ball when lit.
- Return lanes 10,000 light the recapture; an outlane with it lit 25,000 and the ball is recaptured.
- Spinners 2,000 and jackpot +10,000. Pop bumpers 1,000 rising to 10,000 (0x42d484).
- Skill shot (0x432eec): the first part hit after launch: monster target 1 1,000,000 and the plasma field,
  2 starts the experiment, 3 500,000, 4 100,000, 5-7 50,000, anything else 10,000.
- End of ball (0x4300d8): potions x 50,000, experiments x 250,000, monster parts x 100,000, plus the previous
  ball's bonus again (Bonus Hold).

## Not ported yet
- The cork lights on the plunger (0x42d534), the teleporter diverters, the light shows and flask animations,
  the flask level sprites.
- Ported since: the shoot again light, 15 s after each new ball passes plunger_oneway (0x42dab8, 0x42da48 steady
  then flashing for 5 s, 0x42d994); music per experiment, tabd(mode + 1) (0x4304c4).
