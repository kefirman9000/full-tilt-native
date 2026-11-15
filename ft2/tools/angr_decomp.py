# angr_decomp.py: decompile a table exe without Ghidra. Setup: python3 -m venv v && v/bin/pip install angr
# decomp.py EXE OUT LO HI [addr...] : angr-decompile every function in [LO,HI) (or the given addresses) to C
import sys, angr, logging, time
logging.getLogger('angr').setLevel(logging.ERROR); logging.getLogger('cle').setLevel(logging.ERROR)
exe, out, lo, hi = sys.argv[1], sys.argv[2], int(sys.argv[3], 16), int(sys.argv[4], 16)
only = [int(a, 16) for a in sys.argv[5:]]
p = angr.Project(exe, auto_load_libs=False)
t = time.time()
cfg = p.analyses.CFGFast(normalize=True, regions=[(lo, hi)], function_starts=only or None, data_references=True)
p.analyses.CompleteCallingConventions(recover_variables=True, cfg=cfg.model, analyze_callsites=True)
print('cfg', len(cfg.kb.functions), 'functions', round(time.time() - t), 's', file=sys.stderr)
fs = sorted((f for f in cfg.kb.functions.values() if lo <= f.addr < hi and not f.is_simprocedure and not f.is_plt), key=lambda f: f.addr)
with open(out, 'w') as o:
    for f in fs:
        if only and f.addr not in only: continue
        try:
            d = p.analyses.Decompiler(f, cfg=cfg.model)
            code = d.codegen.text if d.codegen else '/* no code */'
        except Exception as e:
            code = '/* decompile failed: %s */' % e
        o.write('// ===== %#x %s\n%s\n\n' % (f.addr, f.name, code)); o.flush()
print('done', len(fs), round(time.time() - t), 's', file=sys.stderr)
