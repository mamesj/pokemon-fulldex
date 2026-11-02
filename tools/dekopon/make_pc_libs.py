#!/usr/bin/env python3
"""Dekopon's core for a PC build of Pokemon Fulldex: writes the CMake file that SSR_DEKOPON_PC_LIBS_FILE names.

Dekopon is built first (README.md here), with DEKOPON_PC=ON and ENABLE_LTO=OFF; this reads the link line
of its citra_pc_probe program for the libraries and their order, then copies them with the symbols
Fulldex's own libraries also define (Teakra, glad) renamed, so that both can be linked into one program.

    make_pc_libs.py <dekopon build folder> <output folder>
"""
import os, re, shlex, shutil, subprocess, sys

build, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
cmd = subprocess.run("ninja -t commands bin/Release/citra_pc_probe | tail -1", shell=True, cwd=build,
                     capture_output=True, text=True, check=True).stdout
libs = []
for t in shlex.split(cmd):
    if t.endswith('.a') and not t.startswith('/usr'):
        libs.append(t if t.startswith('/') else os.path.join(build, t))
    elif t in ('-lpthread', '-lrt', '-ldl'):
        libs.append(t)

def defined(archive):
    r = subprocess.run(['nm', '-g', '--defined-only', archive], capture_output=True, text=True).stdout
    return [l.split()[-1] for l in r.splitlines() if len(l.split()) >= 3]

teakra = next(l for l in libs if l.endswith('libteakra.a'))
glad = next(l for l in libs if l.endswith('libglad.a'))
renamed = sorted({s for s in defined(teakra) if 'Teakra' in s} | set(defined(glad)))
os.makedirs(out, exist_ok=True)
redefine = os.path.join(out, 'redefine.txt')
open(redefine, 'w').write(''.join(f'{s} dk_{s}\n' for s in renamed))
libdir = os.path.join(out, 'libs')
shutil.rmtree(libdir, ignore_errors=True)
os.makedirs(libdir)
copies, items = {}, []
for l in libs:
    if not l.endswith('.a'):
        items.append(l)
        continue
    if l not in copies:
        dst = os.path.join(libdir, f'{len(copies):02d}_{os.path.basename(l)}')
        src = l
        if open(l, 'rb').read(8) == b'!<thin>\n':  # (objcopy cannot read thin archives: make an ordinary one)
            members = subprocess.run(['ar', 't', l], capture_output=True, text=True, check=True).stdout.split()
            src = dst + '.tmp.a'
            subprocess.run(['ar', 'crs', src] + [m if os.path.isabs(m) else os.path.join(os.path.dirname(l), m) for m in members], check=True)
        subprocess.run(['objcopy', '--redefine-syms', redefine, src, dst], check=True)
        if src != l:
            os.remove(src)
        copies[l] = dst
    items.append(copies[l])
cmake = os.path.join(out, 'dekopon_pc_libs.cmake')
open(cmake, 'w').write('set(SSR_DEKOPON_PC_LIBS\n  "-Wl,--start-group"\n' + ''.join(f'  "{i}"\n' for i in items) + '  "-Wl,--end-group"\n)\n')
print(f'{len(renamed)} symbols renamed in {len(copies)} libraries: -DSSR_DEKOPON_PC_LIBS_FILE={cmake}')
