"""Minimal stand-in for the capstone module (16-bit x86 only), built on GNU objdump. Formats operands the way
capstone does (lower case, 'word ptr', '[bx + 0x12]', small numbers in decimal, lcall/ljmp for far transfers)."""
import re, subprocess, tempfile, os

CS_ARCH_X86 = 3
CS_MODE_16 = 2
X86_OP_IMM = 2

class _Op:
    def __init__(self, t, imm=0):
        self.type = t
        self.imm = imm

class _Insn:
    def __init__(self, address, size, mnemonic, op_str, operands, raw):
        self.address, self.size, self.mnemonic, self.op_str, self.operands, self.bytes = \
            address, size, mnemonic, op_str, operands, raw
        pre = []
        for b in raw:
            if b in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0x66, 0x67, 0xf0, 0xf2, 0xf3):
                pre.append(b)
            else:
                break
        self.prefix = (pre + [0, 0, 0, 0])[:4]

def _num(m):
    v = int(m.group(0), 16)
    return str(v) if v < 10 else hex(v)

def _fix_mem(inner):
    inner = re.sub(r'([+-])', r' \1 ', inner)
    return inner

def _convert(mn, ops):
    ops = ops.strip()
    far = False
    if mn in ('call', 'jmp') and ('DWORD PTR' in ops or re.match(r'0x[0-9a-f]+:0x[0-9a-f]+$', ops)):
        far = True
        mn = 'l' + mn
        ops = ops.replace('DWORD PTR ', '')
    if mn == 'pushd':
        mn = 'push'
    if mn == 'pushw':
        mn = 'push'
    ops = ops.replace('QWORD PTR', 'qword ptr').replace('DWORD PTR', 'dword ptr').replace('WORD PTR', 'word ptr') \
             .replace('BYTE PTR', 'byte ptr').replace('FWORD PTR', 'fword ptr')
    # direct memory: ds:0x1234 -> [0x1234], es:0x12 -> es:[0x12]
    ops = re.sub(r'\bds:(0x[0-9a-f]+)', r'[\1]', ops)
    ops = re.sub(r'\b([ecfgs]s):(0x[0-9a-f]+)\b(?!\])', r'\1:[\2]', ops)
    ops = re.sub(r'\[([^\]]*)\]', lambda m: '[' + _fix_mem(m.group(1)) + ']', ops)
    ops = re.sub(r'0x[0-9a-f]+', _num, ops)
    ops = re.sub(r',(?! )', ', ', ops)
    if mn == 'les' or mn == 'lds':
        ops = ops.replace('dword ptr ', 'ptr ')
    return mn, ops

_cache = {}

def _objdump(code, addr):
    key = (bytes(code), addr)
    if key in _cache:
        return _cache[key]
    with tempfile.NamedTemporaryFile(delete=False, suffix='.bin') as f:
        f.write(code)
        name = f.name
    try:
        out = subprocess.run(['objdump', '-D', '-b', 'binary', '-m', 'i8086', '-M', 'intel',
                              '--adjust-vma=%d' % addr, name], capture_output=True, text=True).stdout
    finally:
        os.unlink(name)
    res = []
    for line in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(\S+)\s*(.*)$', line)
        if not m:
            m2 = re.match(r'\s*([0-9a-f]+):\t([0-9a-f ]+?)\s*$', line)
            if m2 and res:  # continuation bytes of the previous instruction
                res[-1][1] += len(m2.group(2).split())
            continue
        a = int(m.group(1), 16)
        size = len(m.group(2).split())
        res.append([a, size, m.group(3), m.group(4)])
    _cache[key] = res
    return res

class Cs:
    def __init__(self, arch, mode):
        self.detail = False

    def disasm(self, code, offset):
        lines = _objdump(code, offset)
        end = offset + len(code)
        for a, size, mn, ops in lines:
            if a + size > end or mn == '(bad)':
                return
            if mn.startswith('rep') and ops and ' ' in ops.strip() is False:
                pass
            raw = bytes(code[a - offset:a - offset + size])
            cmn, cops = _convert(mn, ops)
            operands = []
            if re.match(r'(j|loop|call|jcxz)', cmn) and re.match(r'0x[0-9a-f]+$|\d+$', cops):
                operands.append(_Op(X86_OP_IMM, int(cops, 0)))
            elif cops:
                for o in cops.split(', '):
                    if re.match(r'(0x[0-9a-f]+|\d+)$', o):
                        operands.append(_Op(X86_OP_IMM, int(o, 0)))
                    elif '[' in o:
                        operands.append(_Op(3))
                    else:
                        operands.append(_Op(1))
            yield _Insn(a, size, cmn, cops, operands, raw)
