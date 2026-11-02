#!/usr/bin/env python3
"""Write the program table (programs.c) for the generated program prefixes given on the command line."""
import sys
out, names = sys.argv[1], sys.argv[2:]
L = ['/* GENERATED: list of recompiled programs */', '#include "epic_rt.h"']
L += ['extern const rt_program %s_program;' % n for n in names]
L += ['const rt_program *const rt_programs[] = {'] + ['    &%s_program,' % n for n in names] + ['    0', '};']
open(out, 'w').write('\n'.join(L) + '\n')
