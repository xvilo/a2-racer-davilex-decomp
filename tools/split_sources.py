#!/usr/bin/env python3
"""Split a module's decompiled.c into one file per original source file.

Usage: split_sources.py <module-dir> <A2MENU globals.tsv> <A2MENU modules.tsv> [names.tsv]

Watcom links each object file as one contiguous block, so functions belong to source
files in runs. A function's source file comes from, in order of trust:
  1. A2MENU.DAT's debug info (its module index), for A2MENU itself and for functions
     whose mangled name was carried over from it (names.tsv "mangled" column);
  2. a "<file>.cpp" string the function passes to its assert/log helpers;
  3. the run it sits in: a gap between two functions of the same file joins that file,
     and a gap between two different files goes to the neighbour its calls point to.
The result is written to <module-dir>/src/<file>.c (library objects without a .cpp name
go to src/crt/) and <module-dir>/sources.tsv (address, function, file, evidence).
"""
import csv
import os
import re
import sys

HDR = re.compile(r'^/\* ==== ([0-9a-f]+) (\S+) ==== \*/$')
CPP = re.compile(r'\bs_([A-Za-z0-9_]+?)_cpp_([0-9a-f]{8})\b')
IDENT = re.compile(r'\b[A-Za-z_][A-Za-z0-9_]*\b')


def source_name(path):
    base = path.replace('\\', '/').rsplit('/', 1)[-1].lower()
    if base.endswith('.cpp'):
        return base[:-4]
    return 'crt/' + re.sub(r'[^a-z0-9_]+', '_', re.sub(r'\.obj$', '', base))


def load_menu_modules(globals_tsv, modules_tsv):
    mods = {r['index']: source_name(r['source'])
            for r in csv.DictReader(open(modules_tsv), delimiter='\t')}
    by_addr, by_mangled = [], {}
    for r in csv.DictReader(open(globals_tsv), delimiter='\t'):
        if r['section'] != 'BEGTEXT' or r['module'] not in mods:
            continue
        by_addr.append((int(r['address'], 16), mods[r['module']]))
        # Static helpers can share a mangled name across objects; those stay ambiguous.
        prev = by_mangled.get(r['name'], mods[r['module']])
        by_mangled[r['name']] = mods[r['module']] if prev == mods[r['module']] else None
    by_addr.sort()
    return by_addr, by_mangled


def load_functions(path):
    funcs, cur = [], None
    for line in open(path, encoding='utf-8', errors='replace'):
        m = HDR.match(line.rstrip('\n'))
        if m:
            cur = {'addr': int(m.group(1), 16), 'name': m.group(2), 'body': [line]}
            funcs.append(cur)
        elif cur:
            cur['body'].append(line)
        else:
            continue
    return funcs


def main(mdir, globals_tsv, modules_tsv, names_tsv=None):
    module = os.path.basename(mdir.rstrip('/'))
    funcs = load_functions(f'{mdir}/decompiled.c')
    by_addr, by_mangled = load_menu_modules(globals_tsv, modules_tsv)
    known_cpp = {v for _, v in by_addr}

    # 1. Debug info.
    if module == 'A2MENU.DAT':
        import bisect
        keys = [a for a, _ in by_addr]
        for f in funcs:
            i = bisect.bisect_right(keys, f['addr']) - 1
            if i >= 0:
                f['file'], f['why'] = by_addr[i][1], 'debug-info'
    elif names_tsv:
        mangled = {}
        for r in csv.DictReader((l for l in open(names_tsv) if not l.startswith('#')),
                                delimiter='\t'):
            if r.get('mangled'):
                mangled[int(r['address'], 16)] = r['mangled']
        for f in funcs:
            src = by_mangled.get(mangled.get(f['addr'], ''))
            if src:
                f['file'], f['why'] = src, 'matched'

    # 2. File names in assert/log strings (majority per function).
    for f in funcs:
        if 'file' in f:
            continue
        votes = {}
        for line in f['body']:
            for name, _ in CPP.findall(line):
                votes[name.lower()] = votes.get(name.lower(), 0) + 1
        if votes:
            f['file'] = max(sorted(votes), key=votes.get)
            f['why'] = 'string'

    # 3. Runs: fill gaps from neighbours and call targets.
    file_of = {f['name']: f.get('file') for f in funcs}
    i = 0
    while i < len(funcs):
        if 'file' in funcs[i]:
            i += 1
            continue
        j = i
        while j < len(funcs) and 'file' not in funcs[j]:
            j += 1
        before = funcs[i - 1]['file'] if i > 0 else None
        after = funcs[j]['file'] if j < len(funcs) else None
        if before and before == after:
            for f in funcs[i:j]:
                f['file'], f['why'] = before, 'run'
        else:
            # The boundary lies inside the gap; place it where call targets switch side.
            gap = funcs[i:j]
            names = {f['name'] for f in gap}
            side = []
            for f in gap:
                score = 0
                for ident in IDENT.findall(''.join(f['body'][1:])):
                    if ident in names or ident not in file_of:
                        continue
                    if file_of[ident] == before:
                        score -= 1
                    elif file_of[ident] == after:
                        score += 1
                side.append(score)
            # Best split k: gap[:k] -> before, gap[k:] -> after.
            best_k, best = 0, None
            for k in range(len(gap) + 1):
                s = -sum(side[:k]) + sum(side[k:])
                if before is None:
                    s = 0 if k == 0 else -1
                if after is None:
                    s = 0 if k == len(gap) else -1
                if best is None or s > best:
                    best_k, best = k, s
            fallback = f'unknown_{gap[0]["addr"]:08x}'
            for n, f in enumerate(gap):
                target = before if n < best_k else after
                f['file'] = target or fallback
                f['why'] = 'neighbour' if target else 'unknown'
        for f in funcs[i:j]:
            file_of[f['name']] = f['file']
        i = j

    out = f'{mdir}/src'
    if os.path.isdir(out):
        for root, _, files in os.walk(out):
            for name in files:
                if name.endswith('.c'):
                    os.remove(os.path.join(root, name))
    os.makedirs(out + '/crt', exist_ok=True)
    order = []
    for f in funcs:
        if f['file'] not in order:
            order.append(f['file'])
    for name in order:
        part = [f for f in funcs if f['file'] == name]
        kinds = sorted({f['why'] for f in part})
        with open(f'{out}/{name}.c', 'w', encoding='utf-8') as fh:
            fh.write(f'/* {module}: {name}{"" if name.startswith("crt/") else ".cpp"} — '
                     f'{len(part)} function(s), placed by {", ".join(kinds)}.\n'
                     f' * Generated by tools/split_sources.py from decompiled.c. */\n\n')
            for f in part:
                fh.write(''.join(f['body']).rstrip('\n') + '\n\n\n')
    with open(f'{mdir}/sources.tsv', 'w', encoding='utf-8') as fh:
        fh.write('address\tfunction\tfile\tevidence\n')
        for f in funcs:
            fh.write(f'{f["addr"]:08x}\t{f["name"]}\t{f["file"]}\t{f["why"]}\n')
    counts = {}
    for f in funcs:
        counts[f['why']] = counts.get(f['why'], 0) + 1
    print(f'split_sources: {module}: {len(funcs)} functions into {len(order)} files {counts}')


if __name__ == '__main__':
    main(*sys.argv[1:])
