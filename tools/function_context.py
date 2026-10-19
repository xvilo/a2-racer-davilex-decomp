#!/usr/bin/env python3
"""Summarise functions in a decompiled.c to help name them.

Usage: function_context.py <decompiled.c> [regex-on-name] [--body] [--symbols names.tsv]

For each matching function: entry, line count, callers, callees, string literals and
named globals it touches. --body also prints the decompiled code.
"""
import re
import sys
from collections import defaultdict

HDR = re.compile(r'^/\* ==== ([0-9a-f]+) (\S+) ==== \*/$')
CALL = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')
STR = re.compile(r'\b(s_[A-Za-z0-9_]+|u_[A-Za-z0-9_]+)\b')
KEYWORDS = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'do'}


def parse(path):
    funcs, cur = {}, None
    for line in open(path, encoding='utf-8', errors='replace'):
        m = HDR.match(line.rstrip('\n'))
        if m:
            cur = m.group(2)
            funcs[cur] = {'addr': m.group(1), 'lines': []}
        elif cur:
            funcs[cur]['lines'].append(line.rstrip('\n'))
    return funcs


def main(path, pattern='^FUN_', body=False):
    funcs = parse(path)
    names = set(funcs)
    callers = defaultdict(set)
    info = {}
    for name, f in funcs.items():
        text = '\n'.join(f['lines'])
        sig_end = text.find('{')
        code = text[sig_end:] if sig_end >= 0 else text
        callees = [c for c in dict.fromkeys(CALL.findall(code))
                   if c not in KEYWORDS and c != name and (c in names or not c.startswith(('FUN_', 'DAT_')))]
        for c in callees:
            callers[c].add(name)
        strs = list(dict.fromkeys(STR.findall(code)))
        globs = [g for g in dict.fromkeys(re.findall(r'\b([a-z][a-z0-9_]{2,})\b', code))
                 if g in GLOBALS]
        info[name] = (callees, strs, globs)
    rx = re.compile(pattern)
    for name, f in funcs.items():
        if not rx.search(name):
            continue
        callees, strs, globs = info[name]
        sig = next((l for l in f['lines'] if '(' in l and not l.startswith('/*')), '')
        print(f'## {f["addr"]} {name}  [{len(f["lines"])} lines]  {sig.strip()}')
        print(f'   callers: {", ".join(sorted(callers[name])[:12])}')
        print(f'   callees: {", ".join(callees[:20])}')
        if strs:
            print(f'   strings: {", ".join(strs[:12])}')
        if globs:
            print(f'   globals: {", ".join(globs[:15])}')
        if body:
            print('\n'.join(f['lines']))


GLOBALS = set()  # named data symbols, filled from --symbols

if __name__ == '__main__':
    argv = sys.argv[1:]
    if '--symbols' in argv:
        i = argv.index('--symbols')
        for line in open(argv[i + 1]):
            c = line.split('\t')
            if len(c) > 2 and c[1] == 'data':
                GLOBALS.add(c[2])
        del argv[i:i + 2]
    args = [a for a in argv if a != '--body']
    main(args[0], args[1] if len(args) > 1 else '^FUN_', '--body' in argv)
