#!/usr/bin/env python3
"""Dekopon's core for a Switch build of Pokemon Fulldex: writes the CMake file that SSR_DEKOPON_LIBS_FILE names.

Dekopon is built first (README.md here), with DEKOPON_EMBED=ON and ENABLE_LTO=OFF; this reads the link
line of its dekopon_embed_probe program for the libraries and their order, then
  - joins NXVK's libraries into one object whose symbols are all local except the few Dekopon uses (NXVK
    carries its own copy of Mesa's utility code, which the switch-mesa libraries Fulldex links would clash with),
  - renames the symbols Fulldex's libraries also define (Teakra, get_fattime, the Vulkan entry point).

    make_switch_libs.py <dekopon build folder> <output folder> [--map CONTAINER_PATH=HOST_PATH ...]

--map is for a build made inside a container: its paths (/src/..., /opt/devkitpro/portlibs/...) are given
the folder they have on this machine. devkitPro's tools (aarch64-none-elf-ld, -nm, -objcopy, -ar) must be on PATH.
"""
import os, re, shlex, shutil, subprocess, sys

args = sys.argv[1:]
maps = [a.split('=', 1) for a in args if '=' in a]
pos = [a for a in args if '=' not in a and a != '--map']
build, out = os.path.abspath(pos[0]), os.path.abspath(pos[1])
P = 'aarch64-none-elf-'
cmd = subprocess.run("ninja -t commands bin/Release/dekopon_embed_probe.elf | tail -1", shell=True, cwd=build,
                     capture_output=True, text=True, check=True).stdout

def host(p):
    for c, h in maps:
        if p.startswith(c):
            return h + p[len(c):]
    return p if p.startswith('/') else os.path.join(build, p)

libs = []
for t in shlex.split(cmd):
    if t.endswith('embed_probe.o'):
        continue
    if t.endswith('.a'):
        libs.append(host(t))
    elif t in ('-lnx', '-lc', '-lm', '-lstdc++') or t.startswith('-Wl,'):
        libs.append(t)
for l in libs:
    if l.endswith('.a') and not os.path.exists(l):
        sys.exit(f'missing {l} (--map?)')

def run(*a):
    return subprocess.run(a, capture_output=True, text=True, check=True).stdout

def defined(f):
    return [l.split()[-1] for l in run(P + 'nm', '-g', '--defined-only', f).splitlines() if len(l.split()) >= 3]

os.makedirs(out, exist_ok=True)
nxvk = [l for l in libs if '/externals/nxvk/' in l]
nvk = next(l for l in nxvk if l.endswith('libnvk.a'))
others = [l for l in libs if l.endswith('.a') and '/externals/nxvk/' not in l and 'portlibs' not in l]
joined = os.path.join(out, 'nxvk_all.o')
run(P + 'ld', '-r', '-o', joined, '--whole-archive', nvk, '--no-whole-archive', '--start-group',
    *[l for l in nxvk if l != nvk], '--end-group')
used = {'vk_icdGetInstanceProcAddr'}
for a in others:
    for l in run(P + 'nm', '-u', '-g', a).splitlines():
        p = l.split()
        if len(p) >= 2 and p[0] == 'U':
            used.add(p[1])
keep = sorted(set(defined(joined)) & used)
keep_file = os.path.join(out, 'nxvk_keep.txt')
open(keep_file, 'w').write('\n'.join(keep) + '\n')
local = os.path.join(out, 'nxvk_local.o')
run(P + 'objcopy', f'--keep-global-symbols={keep_file}', joined, local)

teakra = next(l for l in others if l.endswith('libteakra.a'))
renamed = sorted({s for s in defined(teakra) if 'Teakra' in s} | {'vk_icdGetInstanceProcAddr', 'get_fattime'})
redefine = os.path.join(out, 'redefine.txt')
open(redefine, 'w').write(''.join(f'{s} dk_{s}\n' for s in renamed))
libdir = os.path.join(out, 'libs')
shutil.rmtree(libdir, ignore_errors=True)
os.makedirs(libdir)
nxvk_a = os.path.join(libdir, 'libnxvk_all.a')  # (an archive: CMake repeats a plain object on the link line)
shutil.copy(local, os.path.join(libdir, 'nxvk_local.o'))
run(P + 'objcopy', '--redefine-syms', redefine, os.path.join(libdir, 'nxvk_local.o'))
run(P + 'ar', 'rcs', nxvk_a, os.path.join(libdir, 'nxvk_local.o'))
items, done, copies = [], False, {}
for l in libs:
    if '/externals/nxvk/' in l or l in ('-Wl,--whole-archive', '-Wl,--no-whole-archive'):
        continue
    if l == '-Wl,-u,vk_icdGetInstanceProcAddr':
        l = '-Wl,-u,dk_vk_icdGetInstanceProcAddr'
    if l == '-Wl,--start-group' and not done:
        items += [l, nxvk_a]
        done = True
        continue
    if l.endswith('.a') and 'portlibs' not in l:
        if l not in copies:
            dst = os.path.join(libdir, f'{len(copies):02d}_{os.path.basename(l)}')
            run(P + 'objcopy', '--redefine-syms', redefine, l, dst)
            copies[l] = dst
        l = copies[l]
    elif l.endswith('.a'):
        l = l.replace('/opt/devkitpro', '$ENV{DEVKITPRO}') if os.path.basename(l) in ('libz.a', 'libcurl.a') else l
    items.append(l)
cmake = os.path.join(out, 'dekopon_libs.cmake')
open(cmake, 'w').write('set(SSR_DEKOPON_LIBS\n' + ''.join(f'  "{i}"\n' for i in items) + ')\n')
print(f'{len(keep)} NXVK symbols kept global, {len(renamed)} renamed: -DSSR_DEKOPON_LIBS_FILE={cmake}')
