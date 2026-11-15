# The Full Tilt 1 engine, as seen from the table rules

Full Tilt 1 (1995) tables are 16-bit Windows programs. Code segments 1-22 and 24 are the engine (the same in CADET,
DRAGON and PIRATES); segment 23 is the table's rules; the automatic data segment (25 in DRAGON.EXE) holds the rules'
globals and the component pointers. This engine (the Windows XP Space Cadet decompilation) differs in several places
that a faithful port of the rules has to bridge. Everything here was read from the original code; the addresses are
DRAGON.EXE's.

## Tools
- `ft1draft.py EXE --all [helpers.txt]`: every function of segment 23 (rule functions and helpers), with component
  names, table fields and helper names. Follows both switch forms (case-value tables and indexed jump tables).
- `ft1condense.py LISTING STRINGS`: folds push/call sequences into one-line calls. Where two branches share one call
  instruction, the jump shows the arguments it carries ("WITH PUSHED ARGS"); always check those.
- `ft1rules.py`, `ft1lift.py`, `ft1rosetta.py`, `ne.py`: rule table, lifting, names learned from CADET.

## Message codes (Full Tilt 1 -> this engine)
Read from the engine's own message switches (case-value tables found by the `mov cx,N / mov bx,TABLE / mov eax,cs:[bx]`
idiom) and paired with this engine's handler of the same component:
- Lights 0-23: same numbers.
- Light groups 28-50 = this engine's 24-46 (+4). Component group 52 = 48.
- Popup/solo targets 53/54 = Disable/Enable. Blockers 55/56 = Disable/Enable (see blockers below), 57-60 extra.
- Gates 62/63, kickout 64, sink 65 (reset timer).
- To the rules: 61 blocker timeout (this engine: ControlTimerExpired), 67 ball captured, 68 released, 70 timer
  expired, 71 notify timer, 73 spinner loop, 74 collision, 75/76 multiplier, 77 "last hit moved on" (rules only).
- Flipper [1, 2, 1009, 1010, 1011, 1022, 1024, 1026]: 1009 Resume, 1010 LooseFocus, 1011 SetTiltLock, 1022 GameOver,
  1024 PlayerChanged (this engine 1020), 1026 Reset (1024).
- Plunger: 1016 FeedBall, 1017 StartFeedTimer, 1018 LaunchBall, 1019 RelaunchBall.
- Table (15:acbd): 1000-1005 input, 1008-1010, 1011 tilt, 1012, 1013, 1014 new game, 1020, 1022, 1023 player
  switch, 1024, 1026 reset.

## Objects
- Component: +2 UnusedBaseFlag, +3 ActiveFlag (byte), +4 MessageField (dword), +0x24 Control.
- Light: +0x3a LightOnFlag, +0x3c FlasherOnFlag, +0x40 ToggledOnFlag; the three together = light_on().
- Table (data 2266 holds the pointer): +0x5c score, +0x60 score / 10^9, +0x122 multiplier, +0x124 added score,
  +0x12c bonus, +0x130 bonus flag, +0x132 jackpot, +0x136 jackpot flag, +0x138 super jackpot, +0x13c its flag,
  +0x140 ball count, +0x144 extra balls, +0x146 multiball count, +0x14c shoot-again count, +0x15a tilt lock,
  +0xd8 player count, +0xda current player.

## Scoring (15:b8db AddScore)
`score * multiplier + added`, the multiplier being the value itself (1, 2, ...), not an index as in this engine.
While their flags are set, the jackpot (max 10,000,000), super jackpot (max 20,000,000) and bonus (max 5,000,000)
collect the same points. Table reset (1026): multiplier 1, bonus 10,000, jackpot 20,000, super jackpot 20,000,
flags off; new game and player switch keep the jackpots per player. (This engine's New Game sets the jackpot to
500,000 for all Full Tilt tables; the original does not.)

## Text box (18:1270 Display)
`Display(text, seconds, font)`: font 0 the message font (`pbmsg_ft`), 1 the picture font (`font1` in DRAGON.DAT:
dragon, Jackpot, gems, weapons). Many Dragon's Keep messages are single glyphs of the picture font stored in the data
segment (ds:2223 "$" ... ds:2241 "3"; dragon eggs ds:21d2 + n*9 = n "#"). Not supported by this engine yet: it has
one message font. The picture font's glyphs do not map from character 32 at the `font1` group; how the original loads
it still has to be read.

## Blockers (15:3da3)
Not the on/off blocker of this engine: an animated, multi-position part (Dragon's Keep's sword, rocks, the dragon's
jaw, the center post). 55 lower (value >= 0: after that many seconds), 56 raise (the same), 57 set position
(rounded value), 58 move to position with its animation, 59/60 position from the frame count and a sound, 69 timer;
the timeout tells the rules 61. Porting Dragon's Keep needs this behaviour in TBlocker.

## Dragon's Keep segment 23
126 functions: 62 rule functions (75 components) and 64 helpers. 0x06cf scores a hit, 0x072c a given score, 0x0778
is SpecialAddScore, 0x0657 the rules' message handler (the mission system 0xc5d6 hears every message first), 0xc716
dispatches to about fifteen quest handlers (0x7da7 ... 0xc5a9, the largest 0x9fae, 3,767 bytes). The translation is in
`SpaceCadetPinball/ft1_dragon.cpp`, off unless FT2_DEBUG=1 and FT1_DRAGON_FULL=1.

Translated so far (21 of 126): 0657, 06cf, 072c, 0778, 0824, 08a4, 0933, 09e5, 0bdf, 0db4, 0ef0, 0fdd, 107a, 1151,
1187, 151e, 197e, 1c81, 1eda (partly: MessageField holds a sink pointer in the original), 1f2c, 21c5.

Light codes: the original light handler (10:04cc) takes 0-26; 24-26 are the temporary overrides this engine already
has for Full Tilt's Space Cadet (TLightFtTmpOverrideOn/Off/ResetOverride).

Data tables: ds:3514 = the tags of lights 71-75 (indexed by a_rockb's MessageField); ds:21ce "!", ds:21d0 '"'
are picture glyphs too.

Reading the listings: translate from `ft1draft.py --all` (raw); the condensed view hides `les` loads that select
which object a field belongs to (in 1c81 the index is a_rockb's MessageField, not a_kout2's).
