"""
ft1condense.py - shorten an ft1draft.py listing for reading/porting: folds the compiler's push/push/call sequences into
one-line calls (comp->Message(code, value), comp->Play(), Display(text, seconds), helper(args)), names the
message codes (ft1_names.txt plus the codes read from the Full Tilt 1 engine's own switch tables) and drops function
prologues/epilogues. Branches, compares and moves stay as they are. Where the compiler shares one call between
branches, the jump shows the arguments it carries ("WITH PUSHED ARGS"); the call line itself only shows the
fall-through path's arguments.

usage: python ft1condense.py dragon_all.txt [STRINGS.txt] > dragon_condensed.txt
"""
import re, sys, struct

# Full Tilt 1 message code -> this engine's MessageCode (ft1_names.txt + the engine's switch tables: flipper
# [1,2,1009,1010,1011,1022,1024,1026], plunger [..1016-1019..], light groups 28-50, targets 53/54, blockers 55/56,
# blocker timeout 61)
CODES = {1: 'TLightTurnOn', 0: 'TLightTurnOff/Null', 4: 'TLightFlasherStart', 7: 'TLightFlasherStartTimed',
         8: 'TLightTurnOffTimed', 9: 'TLightTurnOnTimed', 11: 'TLightSetOnStateBmpIndex', 12: 'TLightIncOnStateBmpIndex',
         13: 'TLightDecOnStateBmpIndex', 14: 'TLightResetTimed', 15: 'TLightFlasherStartTimedThenStayOn',
         16: 'TLightFlasherStartTimedThenStayOff', 17: 'TLightToggleValue', 18: 'TLightResetAndToggleValue',
         19: 'TLightResetAndTurnOn', 20: 'TLightResetAndTurnOff', 21: 'TLightToggle', 22: 'TLightResetAndToggle',
         23: 'TLightSetMessageField', 2: 'TLightGetLightOnFlag', 3: 'TLightGetFlasherOnFlag',
         53: 'TPopupTargetDisable', 54: 'TPopupTargetEnable', 55: 'TBlockerDisable', 56: 'TBlockerEnable',
         61: 'ControlTimerExpired(blocker 61)', 62: 'TGateDisable', 63: 'TGateEnable', 64: 'TKickoutRestartTimer',
         65: 'TSinkResetTimer', 67: 'ControlBallCaptured', 68: 'ControlBallReleased', 70: 'ControlTimerExpired',
         71: 'ControlNotifyTimerExpired', 73: 'ControlSpinnerLoopReset', 74: 'ControlCollision',
         75: 'ControlEnableMultiplier', 76: 'ControlDisableMultiplier', 77: 'FT1:LastHitMovedOn',
         1009: 'Resume', 1010: 'LooseFocus', 1011: 'SetTiltLock', 1016: 'PlungerFeedBall',
         1017: 'PlungerStartFeedTimer', 1018: 'PlungerLaunchBall', 1019: 'PlungerRelaunchBall', 1022: 'GameOver',
         1024: 'PlayerChanged', 1026: 'Reset'}
for v in range(24, 51):  # light groups: this engine's code + 4
    CODES.setdefault(v + 4 if v + 4 <= 50 else -1, None)
GROUP = ['StepBackward', 'StepForward', 'AnimationBackward', 'AnimationForward', 'LightShowAnimation',
         'GameOverAnimation', 'RandomAnimationSaturation', 'RandomAnimationDesaturation', 'OffsetAnimationForward',
         'OffsetAnimationBackward', 'Reset', 'TurnOnAtIndex', 'TurnOffAtIndex', 'GetOnCount', 'GetLightCount',
         'GetMessage2', 'GetAnimationFlag', 'ResetAndTurnOn', 'ResetAndTurnOff', 'RestartNotifyTimer', 'FlashWhenOn',
         'ToggleSplitIndex', 'StartFlasher']
for i, g in enumerate(GROUP):
    CODES[28 + i] = 'TLightGroup' + g
CODES[52] = 'TComponentGroupResetNotifyTimer'
CODES = {k: v for k, v in CODES.items() if v}

texts = {}
if len(sys.argv) > 2:
    for l in open(sys.argv[2], encoding='latin1'):
        m = re.match(r"(\d+) '(.*)'$", l.rstrip())
        if m: texts[int(m.group(1))] = m.group(2)[:40]

def fval(v):
    try:
        x = int(v, 0)
    except ValueError:
        return v
    if x > 0x1000:
        f = struct.unpack('<f', struct.pack('<I', x & 0xffffffff))[0]
        if abs(f) < 1e7 and f != 0:
            return ('%g' % f) + 'f'
    return str(x)

def code(v):
    try:
        x = int(v, 0)
    except ValueError:
        return v
    return CODES.get(x, 'code%d' % x)

skip = re.compile(r'^\s+[0-9a-f]+  (mov    ax, ss|nop|inc    bp|dec    bp|push   bp|mov    bp, sp|push   ds|'
                  r'mov    ds, ax|push   si|push   di|pop    di|pop    si|pop    ds|pop    bp|lea    sp, \[bp - 2\]|'
                  r'add    sp, 0x[0-9a-f]+|add    sp, \d+|wait|fstp   st\(0\)|sub    sp, 0x[0-9a-f]+|sub    sp, \d+)\s*$')
pushes = []
rc = None
for line in open(sys.argv[1], encoding='latin1'):
    line = line.rstrip('\n')
    if line.startswith('=') or line.startswith('rule function') or line.startswith('helper ') or line.startswith('    case'):
        pushes = []; print(line); continue
    if '>>> C++' in line:
        continue
    m = re.match(r'^\s+([0-9a-f]+)  (\S+)\s+(.*?)\s*(;.*)?$', line)
    if not m:
        print(line); continue
    addr, mn, ops, note = m.group(1), m.group(2), m.group(3), (m.group(4) or '')
    if skip.match(line):
        continue
    if mn == 'push':
        who = re.search(r'; (\w+)', note)
        pushes.append(who.group(1) if who and 'ptr' in ops else ops)
        continue
    if mn == 'push' or (mn == 'les' and 'bx, ptr [0x' in ops and pushes) or (mn == 'mov' and ops.startswith('bx, word ptr es:[bx]')):
        continue
    if mn == 'lcall' and 'virtual Message' in note and len(pushes) >= 3:
        print('      %s  %s->Message(%s, %s)' % (addr, pushes[-1], code(pushes[-2]), fval(pushes[-3]))); pushes = []; continue
    if mn == 'lcall' and 'virtual Play' in note and pushes:
        print('      %s  %s->Play()' % (addr, pushes[-1])); pushes = []; continue
    if mn == 'lcall' and 'virtual get_scoring' in note and len(pushes) >= 2:
        print('      %s  ax = %s->get_scoring(%s)' % (addr, pushes[-1], pushes[-2])); pushes = []; continue
    if mn == 'lcall' and 'pb::get_rc_string' in note and pushes:
        try:
            tid = int(pushes[-1], 0); rc = '%d "%s"' % (tid, texts.get(tid, '?'))
        except ValueError:
            rc = pushes[-1]
        print('      %s  dx:ax = text %s' % (addr, rc)); pushes = []; continue
    if mn == 'lcall' and '->Display' in note and len(pushes) >= 4:
        print('      %s  %s->Display(dx:ax, %s)' % (addr, pushes[-1], fval(pushes[-4]))); pushes = []; continue
    if mn == 'lcall' and 'AddScore' in note and pushes:
        print('      %s  AddScore(%s)' % (addr, ', '.join(reversed(pushes[-3:-1])) or pushes[-1])); pushes = []; continue
    if mn == 'call' and 'helper' in note:
        args = [a for a in reversed(pushes) if a != 'cs']
        print('      %s  %s(%s)' % (addr, note.split('helper ')[1].strip(), ', '.join(args))); pushes = []; continue
    if mn == 'lcall':
        print('      %s  CALL %s  [pushed: %s]' % (addr, note.strip('; '), ', '.join(reversed(pushes)))); pushes = []; continue
    if mn.startswith('j') and pushes:
        # the compiler shares one call between branches: these arguments go to the call at the jump target
        print('      %s  %-6s %-34s ; WITH PUSHED ARGS (last first): %s' % (addr, mn, ops, ', '.join(reversed(pushes))))
        if mn == 'jmp':
            pushes = []
        continue
    print('      %s  %-6s %-34s %s' % (addr, mn, ops, note))
