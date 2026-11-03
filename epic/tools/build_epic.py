#!/usr/bin/env python3
"""
build_epic.py - build EpicPinball from your own Epic Pinball files.

Run by the launcher's "Compile Epic Pinball tables" button (or by hand):

    python epic/tools/build_epic.py --game tables/EpicPinball --out .

1. recompiles PINBALL.EXE, SETUP.EXE, EP1-EP13.EXE and the MASI drivers (MDRV000R-005R.MUS) to C
   (epic/tools/eprecomp.py) into epic/gen-local/ - this folder stays on your computer
2. compiles them with the runtime (epic/runtime) using MSVC (Visual Studio / Build Tools), MinGW-w64 or gcc/clang
3. writes EpicPinball(.exe) next to the launcher

Needs Python 3, the capstone module (installed automatically with pip if missing) and a C compiler.
Progress goes to stdout; the launcher shows it and saves it in EpicBuild.log.
"""
import argparse, glob, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
EPIC = os.path.dirname(HERE)
RUNTIME = os.path.join(EPIC, 'runtime')
SDK = os.path.join(EPIC, 'sdk')          # SDL2 headers and import library shipped with the Windows release
if not os.path.isdir(SDK) and os.path.isdir(os.path.join(EPIC, '..', 'Libs', 'SDL2')):
    SDK = os.path.join(EPIC, '..', 'Libs', 'SDL2')   # source checkout: the SDL2 development package used by CMake

def say(msg):
    print(msg, flush=True)

def fail(msg):
    say('ERROR: ' + msg)
    sys.exit(1)

# Progress for the launcher's progress bar: "PROGRESS <0..1> <what is happening>".
# Recompiling the 21 programs is the first 40%, compiling the C files the next 58%, linking the rest.
def progress(fraction, text):
    say('PROGRESS %.3f %s' % (max(0.0, min(1.0, fraction)), text))

def find_file(folder, name):
    for f in os.listdir(folder):
        if f.upper() == name.upper():
            return os.path.join(folder, f)
    return None

def ensure_capstone():
    try:
        import capstone  # noqa: F401
        return
    except ImportError:
        pass
    say('Installing the capstone disassembler module (pip)...')
    r = subprocess.run([sys.executable, '-m', 'pip', 'install', '--user', 'capstone'])
    if r.returncode != 0:
        fail('could not install capstone. Run: python -m pip install capstone')
    import site, importlib
    sys.path.append(site.getusersitepackages())
    importlib.invalidate_caches()

def generate(game, gen):
    os.makedirs(gen, exist_ok=True)
    progs = [('PINBALL.EXE', 'pinball'), ('SETUP.EXE', 'setup')]
    progs += [('EP%d.EXE' % n, 'ep%d' % n) for n in range(1, 14)]
    progs += [('MDRV%03dR.MUS' % n, 'mdrv%03dr' % n) for n in range(6)]
    names = []
    for k, (fname, prefix) in enumerate(progs):
        src = find_file(game, fname)
        if not src:
            fail('%s is missing from %s' % (fname, game))
        say('[%d/%d] recompiling %s' % (k + 1, len(progs), fname))
        progress(0.40 * k / len(progs), 'Recompiling %s (%d of %d)' % (fname, k + 1, len(progs)))
        seeds = os.path.join(HERE, 'seeds', os.path.splitext(fname)[0].upper() + '.txt')
        cmd = [sys.executable, os.path.join(HERE, 'eprecomp.py'), src, prefix, os.path.join(gen, prefix + '.c')]
        if os.path.exists(seeds):
            cmd += ['--seeds', seeds]
        r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        if r.returncode != 0:
            say(r.stdout)
            fail('recompiling %s failed' % fname)
        names.append(prefix)
    subprocess.check_call([sys.executable, os.path.join(HERE, 'mkprograms.py'), os.path.join(gen, 'programs.c')] + names)

def sources(gen):
    return sorted(glob.glob(os.path.join(RUNTIME, '*.c'))) + sorted(glob.glob(os.path.join(gen, '*.c')))

def vs_env():
    """Environment of the newest Visual Studio / Build Tools with the C++ x64 tools, or None."""
    pf = os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')
    vswhere = os.path.join(pf, 'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    if not os.path.exists(vswhere):
        return None
    path = subprocess.run([vswhere, '-latest', '-products', '*', '-requires',
                           'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'],
                          stdout=subprocess.PIPE, universal_newlines=True).stdout.strip()
    vcvars = os.path.join(path, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat') if path else ''
    if not os.path.exists(vcvars):
        return None
    out = subprocess.run('"%s" >nul && set' % vcvars, shell=True, stdout=subprocess.PIPE,
                         universal_newlines=True).stdout
    env = {}
    for line in out.splitlines():
        if '=' in line:
            k, v = line.split('=', 1)
            env[k] = v
    return env

def compile_all(srcs, make_cmd, env=None):
    """Compile every source in parallel, reporting progress as each file finishes."""
    jobs = max(1, os.cpu_count() or 2)
    pending = list(srcs)
    running = []
    done = 0
    total = len(srcs)
    progress(0.40, 'Compiling C files (0 of %d)' % total)
    while pending or running:
        while pending and len(running) < jobs:
            src = pending.pop(0)
            running.append((src, subprocess.Popen(make_cmd(src), env=env)))
        src, proc = running.pop(0)
        if proc.wait() != 0:
            for _, p in running:
                p.kill()
            fail('compiling %s failed' % os.path.basename(src))
        done += 1
        say('compiled %s' % os.path.basename(src))
        progress(0.40 + 0.58 * done / total, 'Compiling C files (%d of %d)' % (done, total))

def build_msvc(env, gen, out_exe):
    obj = os.path.join(gen, 'obj')
    os.makedirs(obj, exist_ok=True)
    inc = ['/I' + RUNTIME, '/I' + os.path.join(SDK, 'include')]
    srcs = sources(gen)
    say('Compiling %d files with MSVC (this takes a few minutes)...' % len(srcs))
    compile_all(srcs, lambda src: ['cl', '/nologo', '/O2', '/W1', '/c'] + inc +
                ['/Fo' + os.path.join(obj, os.path.splitext(os.path.basename(src))[0] + '.obj'), src], env)
    progress(0.99, 'Linking EpicPinball')
    objs = glob.glob(os.path.join(obj, '*.obj'))
    lib = os.path.join(SDK, 'lib', 'x64', 'SDL2.lib')
    r = subprocess.run(['link', '/nologo', '/SUBSYSTEM:WINDOWS', '/OUT:' + out_exe] + objs + [lib, 'user32.lib', 'shell32.lib'],
                       env=env)
    if r.returncode != 0:
        fail('linking failed')

def build_gcc(cc, gen, out_exe):
    srcs = sources(gen)
    flags = ['-std=c99', '-O2', '-w', '-I' + RUNTIME]
    libs = []
    if os.name == 'nt':
        flags += ['-I' + os.path.join(SDK, 'include'), '-mwindows']
        libs = [os.path.join(SDK, 'lib', 'x64', 'SDL2.lib')]
    else:
        cfg = shutil.which('sdl2-config')
        if not cfg:
            fail('SDL2 development files not found (install libsdl2-dev or SDL2 for your system)')
        flags += subprocess.check_output([cfg, '--cflags'], universal_newlines=True).split()
        libs = subprocess.check_output([cfg, '--libs'], universal_newlines=True).split()
    obj = os.path.join(gen, 'obj')
    os.makedirs(obj, exist_ok=True)
    say('Compiling %d files with %s (this takes a few minutes)...' % (len(srcs), cc))
    objpath = lambda src: os.path.join(obj, os.path.splitext(os.path.basename(src))[0] + '.o')
    compile_all(srcs, lambda src: [cc] + flags + ['-c', src, '-o', objpath(src)])
    progress(0.99, 'Linking EpicPinball')
    objs = [objpath(src) for src in srcs]
    r = subprocess.run([cc] + objs + ['-o', out_exe] + libs + (['-mwindows'] if os.name == 'nt' else []))
    if r.returncode != 0:
        fail('linking failed')

def refresh_windows_path():
    """Tools installed after the launcher started ("Install build tools") are not on its PATH yet."""
    if os.name != 'nt':
        return
    extra = []
    try:
        import winreg
        for root, key in ((winreg.HKEY_CURRENT_USER, 'Environment'),
                          (winreg.HKEY_LOCAL_MACHINE, r'SYSTEM\CurrentControlSet\Control\Session Manager\Environment')):
            try:
                with winreg.OpenKey(root, key) as k:
                    extra += os.path.expandvars(winreg.QueryValueEx(k, 'Path')[0]).split(';')
            except OSError:
                pass
    except ImportError:
        pass
    local = os.environ.get('LOCALAPPDATA', '')
    extra.append(os.path.join(local, 'Microsoft', 'WinGet', 'Links'))
    # WinLibs installed by winget lives in the WinGet packages folder
    extra += glob.glob(os.path.join(local, 'Microsoft', 'WinGet', 'Packages', 'BrechtSanders.WinLibs*', 'mingw64', 'bin'))
    os.environ['PATH'] = os.pathsep.join([p for p in extra if p] + [os.environ.get('PATH', '')])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', required=True, help='folder with PINBALL.EXE, EP1-EP13 and the other game files')
    ap.add_argument('--out', required=True, help='folder to write EpicPinball(.exe) to (next to the launcher)')
    a = ap.parse_args()
    if not os.path.isdir(a.game) or not find_file(a.game, 'PINBALL.EXE'):
        fail('no Epic Pinball install found in %s' % a.game)
    progress(0.0, 'Getting ready')
    refresh_windows_path()
    ensure_capstone()
    sys.path.insert(0, HERE)
    gen = os.path.join(EPIC, 'gen-local')
    generate(a.game, gen)
    exe = os.path.join(a.out, 'EpicPinball.exe' if os.name == 'nt' else 'EpicPinball')
    tmp = exe + '.new'
    if os.name == 'nt':
        env = vs_env()
        if env:
            build_msvc(env, gen, tmp)
        elif shutil.which('gcc'):
            build_gcc('gcc', gen, tmp)
        else:
            fail('no C compiler found. Install "Build Tools for Visual Studio" (C++ workload) or MinGW-w64, then try again.')
        sdl = os.path.join(a.out, 'SDL2.dll')
        if not os.path.exists(sdl) and os.path.exists(os.path.join(SDK, 'lib', 'x64', 'SDL2.dll')):
            shutil.copy(os.path.join(SDK, 'lib', 'x64', 'SDL2.dll'), sdl)
    else:
        cc = shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
        if not cc:
            fail('no C compiler found (install gcc or clang)')
        build_gcc(cc, gen, tmp)
    os.replace(tmp, exe)
    progress(1.0, 'Done')
    say('DONE: built %s' % exe)

if __name__ == '__main__':
    main()
