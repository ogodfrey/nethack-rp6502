#!/usr/bin/env python3
"""Compile every nethack_objs module to llvm-mos assembly (-S).

    ovlasm.py BUILDDIR OUTDIR

BUILDDIR is a CMake build directory configured with CMAKE_EXPORT_COMPILE_COMMANDS
(the llvm-mos presets do). Each nethack_objs entry is rerun with the same flags,
-c replaced by -S, and written to OUTDIR/<module>.s, where <module> is the source
path with '/' replaced by '_' (e.g. src_allmain.s). The packer (ovlpack.py)
reads these files; the object files are not used.
"""
import concurrent.futures
import json
import os
import shlex
import subprocess
import sys

TARGET = 'nethack_objs.dir/'


def module_name(output):
    rel = output.split(TARGET, 1)[1]
    if rel.startswith('gen/msgsrc/'):
        rel = rel[len('gen/msgsrc/'):]
    if rel.endswith('.c.obj'):
        rel = rel[:-len('.c.obj')]
    return rel.replace('/', '_')


def asm_command(entry, outdir):
    args = shlex.split(entry['command'])
    out = []
    skip = False
    for i, a in enumerate(args):
        if skip:
            skip = False
            continue
        if a == '-o':
            skip = True
            continue
        if a == '-c' or a == entry['file']:
            continue
        out.append(a)
    name = module_name(entry['output'])
    path = os.path.join(outdir, name + '.s')
    out += ['-S', '-o', path, entry['file']]
    return out, path


def main():
    builddir, outdir = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    with open(os.path.join(builddir, 'compile_commands.json')) as f:
        entries = json.load(f)
    mods = [e for e in entries if TARGET in e.get('output', '')]
    if not mods:
        raise SystemExit('no nethack_objs entries in compile_commands.json')
    os.makedirs(outdir, exist_ok=True)
    jobs = [asm_command(e, outdir) + (e['directory'],) for e in mods]

    def run(job):
        argv, path, cwd = job
        r = subprocess.run(argv, cwd=cwd, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
        return path, r.returncode, r.stdout.decode('utf-8', 'replace')

    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as ex:
        for path, rc, log in ex.map(run, jobs):
            if rc != 0:
                failed += 1
                print('FAILED', path)
                print(log[-2000:])
    print('modules: %d  failed: %d  asm in %s' % (len(jobs), failed, outdir))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
