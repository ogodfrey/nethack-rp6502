#!/usr/bin/env python3
"""Turn cgprof.out (from a PROFILE=1 host build) into a call graph.

    cgreport.py NETHACK CGPROF > callgraph.tsv

Output lines: caller<TAB>callee<TAB>count, where functions are named
module/function (module = source file without .c) so static functions with
the same name in different files stay distinct.  Calls from uninstrumented
code (libc callbacks such as qsort) have the caller "-".
"""
import collections
import os
import subprocess
import sys


def main():
    exe, prof = sys.argv[1], sys.argv[2]
    edges = collections.Counter()
    addrs = set()
    with open(prof) as f:
        for line in f:
            site, fn, n = line.split()
            site, fn = int(site, 16), int(fn, 16)
            edges[site, fn] += int(n)
            addrs.add(site)
            addrs.add(fn)
    addrs = sorted(addrs)
    out = subprocess.run(['addr2line', '-f', '-e', exe] +
                         ['%x' % a for a in addrs],
                         capture_output=True, text=True, check=True).stdout
    lines = out.splitlines()
    names = {}
    for i, a in enumerate(addrs):
        func, loc = lines[2 * i], lines[2 * i + 1]
        src = loc.split(':')[0]
        if func == '??' or src == '??':
            names[a] = '-'
        else:
            mod = os.path.basename(src)
            names[a] = '%s/%s' % (mod[:-2] if mod.endswith('.c') else mod,
                                  func)
    graph = collections.Counter()
    for (site, fn), n in edges.items():
        graph[names[site], names[fn]] += n
    for (a, b), n in sorted(graph.items(), key=lambda x: -x[1]):
        print('%s\t%s\t%d' % (a, b, n))


if __name__ == '__main__':
    main()
