# Captain Hero rules (recovered from hero.exe)

Rule table: 133 entries (`ft2/hero_controls.txt`, made by `ft2/tools/ft2rules.py hero.exe 0x401a38`). The rule
functions were read from a decompile of hero.exe's control code (0x401000-0x409000, source `hero\src\control.cpp`).
Ghidra could not be used, so the C came from angr's decompiler; its asserts keep many original variable names
(`which_mode`, `MODE_ELASTIC`, `dynamite_dropped`, `current_mystman_bumper`, `strength_setting`).
The port is the Captain Hero section of `SpaceCadetPinball/ft2_control.cpp`; comments give the original function
for each rule.

## Engine conventions in hero.exe
- Helpers: 0x401a38 score (component, points; pops times 0x405de8), 0x40a2fc show text (box, text id, ..., seconds),
  0x43164c set timer (seconds, callback), 0x431844 kill timer, 0x40f41c assert, 0x434894 strcmp on part names.
- Table fields: +0x141 Jackpot, +0x149 Super Jackpot, +0x15d extra balls.
- Messages: 76 hit, 1017 reset (new ball), 86 ramp made, 67 sink eject, 0x11 light on/off, 28 light off, 4 flash.

## Battles (0x43d088; start 0x40787c, end 0x407e08)
Modes: 1 Quake, 2 Mad Scientist, 3 Mystman, 4 Slime Man, 5 Mr. Pling (`MODE_ELASTIC`), 6 Evil Twin, 7 game over
(only a debug key starts it).
- The scoop s_sink01 (0x406580) with no battle on: 0x406364 lists the villains not captured yet; Evil Twin only
  when all five are. One choice starts at once, otherwise "Battle %s?" (3063): left flipper next, right flipper
  accept, 20 s timeout accepts.
- Start: 100,000, battle count +1, the mystery light (light_74) flashes for 5 s (0x40496c). Battles 1-5 run a
  60 s timer (0x42700000); the display counts down every 0.1 s (0x40780c, box 14). Timeout = 0x40858c = lost.
- Scoop during a battle: Mad Scientist's battle is won there; in the others it restarts the 60 s ("Extend
  Battle", 3033).
- Won: captures +1, Super Jackpot += 250,000 x multiplier (0x4077c8), the Jackpot is paid ("JACKPOT!!!" 3064) and
  goes back to 500,000. Each villain's capture text 3069..3077.
1. Quake: the nine drop target pairs (0x403aec), 75,000 each while lit.
2. Mad Scientist: shoot the scoop again.
3. Mystman: a henchman (a_bump04..07) comes out; 5 hits (10,000 x hit) capture him.
4. Slime Man: 8 pop bumper hits (0x4023c8, countdown text 3074).
5. Mr. Pling: his three walls a_rebo03/04/05 are off except in his battle (their active flag, written by
   0x405944, 0x405ac4 and 0x407e08). 8 hits on a_rebo04, 50,000 each. In the original a second ball is sent up
   s_ramp02 into him first (ball +0x140 = 5); the port switches the walls on when the battle starts.
6. Evil Twin ("Blow Up Krypton-@", 3079): five pieces of Krypton-@ (lights 73, 71, 93, 95, 94, text 3080). The
   dynamite drop targets a_drop18..21 (0x402e3c) and s_kout01 (0x406294) each blow up the next piece, 200,000
   (0x406c14). s_kout01 also lights the Super Jackpot (0x441a44 = 1, light_49). Three downtown ramp shots
   (0x408638) collect it: "SUPER JACKPOT!!!" (3065), the s_kout01 award three times (0x4061f0: 75,000 each and a
   text, not ported yet), "eject" to s_sink03/05/04/01, balls-in-play +4 (table +0x161), the Super Jackpot back to
   1,000,000. The port adds three balls at the holes: an approximation until the ball source is read.
   No timer: the battle ends with the ball, and either way all villains come back (0x406b08).

- Music (0x40787c): the mode's track, tabf(mode + 1); tabf1 with no battle, tabf8 for the game over mode.

## Jackpots (0x40764c)
- Jackpot: 500,000 at the start, max 5,000,000. +5,000 for a target (0x4076d8: drop and spot targets), +10,000
  for a hole (0x407738: s_sink03/04/05/08/09/13/14/15/20). The skill shot can add 500,000.
- Super Jackpot: 1,000,000 at the start, max 20,000,000; only grows during a battle (0x40756c): targets 30,000,
  holes 40,000, captures 250,000, each times the multiplier (0x44160c, 1-5).

## Holes and other rules
- s_sink03 (0x404840): with the mystery light on, the mystery award (0x404768, weights 50/25/15/5/3/2 %): 50,000,
  100,000, 1 Million, All Buildings Repaired, Max Bumpers, Extra Ball (only when none is waiting). Otherwise the
  Super Jackpot multiplier +1 up to 5 (3031). The multiplier goes back to 1 with each ball.
- s_sink10 (0x404b28): 25,000 and lights the extra ball (light_59, "Extra Ball Available" 3055), once per game.
- s_sink14 (0x404258): 50,000 and the extra ball when lit ("EXTRA BALL!!!" 3004).
- s_sink04 / s_sink05 (0x404470 / 0x404344): 60,000 and their light; both lit: 125,000 and the pop bumpers +1,000
  (up to 5,000, "Bumpers Boosted!" 3005).
- s_kout02 "Be A Hero!" (0x4045a4): 30,000, 10,000 more each time up to 150,000, and a good deed (texts 3007-3024).
- Downtown ramp (0x405bf8): not counted on the launch; 30,000, and 30,000 more for each shot while light_49 is on
  (10 s).
- Pop bumpers (0x4022f8 / 0x4023c8): 1,000, +1,000 every 8 hits up to 5,000.

## Buildings (0x449928, ten buildings btrig01..10)
- Parts that throw the ball write a mark on it (ball +0x140): lit spot targets a_spot09..16 (1), drop targets
  a_drop29..46 (2), kickouts s_kout03..05 (4), Mr. Pling's walls (5), the magnet pockets s_kout06..10 (6).
- A building trigger crossed by a marked ball (0x408ac8): damage +1, max 2 (0x40895c); mark 6 wrecks it (0x408a1c).
  "Hit Bumpers to Repair Buildings" (3001).
- Pop bumpers repair (0x4023c8 -> 0x408940): on half their hits and whenever their value steps up, one random
  damaged building per 1,000 of their value is fully repaired (0x40888c).
- The port clears the mark after 4 s or when the ball hits another part (the original clears it in engine code
  not yet read).

## End of ball (0x406f58)
50,000 per building not wrecked (0x408b08) + 100,000 per battle + 250,000 per capture this ball
(text 3050). Drain rule 0x4070bc: extra ball, ball saver s_sink30, then the bonus.

## Approximations in the port (marked APPROXIMATION in ft2_control.cpp)
- Super Jackpot multiball: three balls added at the holes (see Evil Twin above).
- Building mark: cleared after 4 s or the next hit (original: engine code not read).
- Pop repairs on "half the hits": unclear condition in 0x4023c8.
- Mr. Pling: walls switch on with the battle (original: a second ball goes up s_ramp02 first).
- Texts: battle prompts, "RAMP COMBO", the side panel lines and the bonus line are this port's wording; the
  original shows its own texts (ids in this file) in its own display boxes.

## Not ported yet
- Mr. Pling's second ball travelling up s_ramp02 into him; the walls simply switch on with the battle.
- The s_kout01 award (0x4061f0: three steps, 75,000 each) given at the Super Jackpot.
- The countdown and capture texts in their own display boxes (the port uses the info text and side panel).
- Playfield multiplier 0x405de8 (pops x3), table multiplier in the bonus (table +0x12d), Super Strength / Speed /
  Flight (3034-3036), Quake's light bars and Slime Man's slime patches (pictures only).
