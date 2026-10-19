#!/usr/bin/env python3
"""Suggest names for unnamed functions from shared string literals.

Usage: match_by_strings.py <src-module-dir> <dst-module-dir>

Both arguments are decompiled/<module> directories (decompiled.c + strings.tsv). For each
FUN_ in the destination, the string literals it references are compared with every named
source function; a suggestion is printed when one source function shares the most strings
(ties are reported as ambiguous). Output: dst_address, suggestion, shared/own counts.
"""
import re
import sys

HDR = re.compile(r'^/\* ==== ([0-9a-f]+) (\S+) ==== \*/$')
SREF = re.compile(r'\b(?:s|u)_[A-Za-z0-9_]*?_([0-9a-f]{8})\b')


def load(d):
    strings = {}
    for line in open(f'{d}/strings.tsv', encoding='utf-8', errors='replace'):
        c = line.rstrip('\n').split('\t')
        if len(c) == 3 and c[0] != 'address':
            strings[c[0].lower().zfill(8)] = c[2]
    funcs, cur = {}, None
    for line in open(f'{d}/decompiled.c', encoding='utf-8', errors='replace'):
        m = HDR.match(line.rstrip('\n'))
        if m:
            cur = (m.group(1), m.group(2))
            funcs[cur] = set()
        elif cur:
            for a in SREF.findall(line):
                v = strings.get(a)
                # Skip trivially short or format-only strings: they are shared by chance.
                if v and len(re.sub(r'%[-0-9.]*[a-z]|[^A-Za-z]', '', v)) >= 5:
                    funcs[cur].add(v)
    return funcs


def main(src, dst):
    s, d = load(src), load(dst)
    by_string = {}
    for (addr, name), strs in s.items():
        if name.startswith('FUN_'):
            continue
        for v in strs:
            by_string.setdefault(v, set()).add(name)
    for (addr, name), strs in sorted(d.items()):
        if not name.startswith('FUN_') or not strs:
            continue
        score = {}
        for v in strs:
            for n in by_string.get(v, ()):
                score[n] = score.get(n, 0) + 1
        if not score:
            continue
        best = max(score.values())
        top = sorted(n for n, c in score.items() if c == best)
        tag = top[0] if len(top) == 1 else 'AMBIGUOUS:' + ','.join(top[:4])
        print(f'{addr}\t{name}\t{tag}\t{best}/{len(strs)}')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
