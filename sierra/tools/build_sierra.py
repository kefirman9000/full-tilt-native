#!/usr/bin/env python3
"""
build_sierra.py - build UltraPinball from your own copy of Sierra's 3-D Ultra Pinball (1995).

Run by the launcher's "Compile 3-D Ultra Pinball" button (or by hand):

    python sierra/tools/build_sierra.py --game tables/UltraPinball --out .

1. recompiles PINBALL.EXE to C (sierra/tools/sprecomp.py) into sierra/gen-local/ - this folder stays on your
   computer: the generated code is a translation of Sierra's program
2. compiles it with the runtime (sierra/runtime) using MSVC (Visual Studio / Build Tools), MinGW-w64 or gcc/clang
3. writes UltraPinball(.exe) next to the launcher

Needs Python 3, the capstone module (installed automatically with pip if missing) and a C compiler; the same tools
as Epic Pinball, so the launcher's "Install build tools" button covers both. Progress goes to stdout as
"PROGRESS <0..1> <step>" lines; the launcher shows them and saves the output in SierraBuild.log.
"""
import argparse, glob, os, shutil, subprocess, sys, struct

HERE = os.path.dirname(os.path.abspath(__file__))
SIERRA = os.path.dirname(HERE)
ROOT = os.path.dirname(SIERRA)
RUNTIME = os.path.join(SIERRA, 'runtime')
PARTS = 8                                   # generated C files, compiled in parallel

# Compiler detection, PATH refresh and parallel compiling are shared with the Epic Pinball build.
sys.path.insert(0, os.path.join(ROOT, 'epic', 'tools'))
try:
    import build_epic as be
except ImportError:
    print('ERROR: the epic folder (shared build tools) is missing next to the sierra folder', flush=True)
    sys.exit(1)
SDK = be.SDK                                # SDL2 headers and import library (release: epic/sdk)
say, fail, progress, find_file = be.say, be.fail, be.progress, be.find_file

def is_ultra_pinball(exe):
    """PINBALL.EXE of 3-D Ultra Pinball: a 32-bit Windows program that loads Sierra's SOS sound library."""
    d = open(exe, 'rb').read()
    if d[:2] != b'MZ' or len(d) < 0x40:
        return False
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    return d[pe:pe + 4] == b'PE\0\0' and b'sos9502' in d.lower() and b'UPINBALL' in d.upper()

def generate(game, gen):
    os.makedirs(gen, exist_ok=True)
    for old in glob.glob(os.path.join(gen, '*.c')):
        os.remove(old)
    src = find_file(game, 'PINBALL.EXE')
    say('recompiling PINBALL.EXE')
    progress(0.02, 'Recompiling PINBALL.EXE')
    cmd = [sys.executable, os.path.join(HERE, 'sprecomp.py'), src, 'upb', os.path.join(gen, 'upb.c'), '--parts', str(PARTS)]
    seeds = os.path.join(HERE, 'seeds', 'PINBALL.txt')
    if os.path.exists(seeds):
        cmd += ['--seeds', seeds]
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    say(r.stdout.strip())
    if r.returncode != 0:
        fail('recompiling PINBALL.EXE failed')

def sources(gen):
    return sorted(glob.glob(os.path.join(RUNTIME, '*.c'))) + sorted(glob.glob(os.path.join(gen, '*.c')))

def build_msvc(env, gen, out_exe):
    obj = os.path.join(gen, 'obj')
    os.makedirs(obj, exist_ok=True)
    inc = ['/I' + RUNTIME, '/I' + os.path.join(SDK, 'include')]
    srcs = sources(gen)
    say('Compiling %d files with MSVC (this takes a few minutes)...' % len(srcs))
    cl = be.msvc_tool(env, 'cl')
    be.compile_all(srcs, lambda src: [cl, '/nologo', '/O2', '/W1', '/std:c11', '/c'] + inc +
                   ['/Fo' + os.path.join(obj, os.path.splitext(os.path.basename(src))[0] + '.obj'), src], env)
    progress(0.99, 'Linking UltraPinball')
    objs = [os.path.join(obj, os.path.splitext(os.path.basename(s))[0] + '.obj') for s in srcs]
    lib = os.path.join(SDK, 'lib', 'x64', 'SDL2.lib')
    r = subprocess.run([be.msvc_tool(env, 'link'), '/nologo', '/SUBSYSTEM:WINDOWS', '/OUT:' + out_exe] + objs + [lib, 'user32.lib', 'shell32.lib'],
                       env=env)
    if r.returncode != 0:
        fail('linking failed')

def build_gcc(cc, gen, out_exe):
    srcs = sources(gen)
    flags = ['-std=c99', '-O2', '-w', '-I' + RUNTIME]
    libs = []
    if os.name == 'nt':
        flags += ['-I' + os.path.join(SDK, 'include')]
        libs = [os.path.join(SDK, 'lib', 'x64', 'SDL2.lib'), '-mwindows']
    else:
        cfg = shutil.which('sdl2-config')
        if not cfg:
            fail('SDL2 development files not found (install libsdl2-dev or SDL2 for your system)')
        flags += subprocess.check_output([cfg, '--cflags'], universal_newlines=True).split()
        libs = subprocess.check_output([cfg, '--libs'], universal_newlines=True).split() + ['-lm']
    obj = os.path.join(gen, 'obj')
    os.makedirs(obj, exist_ok=True)
    say('Compiling %d files with %s (this takes a few minutes)...' % (len(srcs), cc))
    objpath = lambda src: os.path.join(obj, os.path.splitext(os.path.basename(src))[0] + '.o')
    be.compile_all(srcs, lambda src: [cc] + flags + ['-c', src, '-o', objpath(src)])
    progress(0.99, 'Linking UltraPinball')
    r = subprocess.run([cc] + [objpath(s) for s in srcs] + ['-o', out_exe] + libs)
    if r.returncode != 0:
        fail('linking failed')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', required=True, help='folder with PINBALL.EXE, RESOURCE.MAP and RESOURCE.001/.002')
    ap.add_argument('--out', required=True, help='folder to write UltraPinball(.exe) to (next to the launcher)')
    a = ap.parse_args()
    exe = find_file(a.game, 'PINBALL.EXE') if os.path.isdir(a.game) else None
    if not exe or not find_file(a.game, 'RESOURCE.MAP'):
        fail('no 3-D Ultra Pinball found in %s (PINBALL.EXE and RESOURCE.MAP are needed)' % a.game)
    if not is_ultra_pinball(exe):
        fail('%s is not 3-D Ultra Pinball\'s PINBALL.EXE' % exe)
    progress(0.0, 'Getting ready')
    be.refresh_windows_path()
    be.ensure_capstone()
    gen = os.path.join(SIERRA, 'gen-local')
    generate(a.game, gen)
    out = os.path.join(a.out, 'UltraPinball.exe' if os.name == 'nt' else 'UltraPinball')
    tmp = out + '.new'
    if os.name == 'nt':
        env = be.vs_env()
        if env:
            build_msvc(env, gen, tmp)
        elif shutil.which('gcc'):
            build_gcc('gcc', gen, tmp)
        else:
            fail('no C compiler found. Click "Install build tools" in the launcher, or install "Build Tools for '
                 'Visual Studio" (C++ workload) or MinGW-w64, then try again.')
        sdl = os.path.join(a.out, 'SDL2.dll')
        if not os.path.exists(sdl) and os.path.exists(os.path.join(SDK, 'lib', 'x64', 'SDL2.dll')):
            shutil.copy(os.path.join(SDK, 'lib', 'x64', 'SDL2.dll'), sdl)
    else:
        cc = shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
        if not cc:
            fail('no C compiler found (install gcc or clang)')
        build_gcc(cc, gen, tmp)
    os.replace(tmp, out)
    progress(1.0, 'Done')
    say('DONE: built %s' % out)

if __name__ == '__main__':
    main()
