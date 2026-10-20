#!/usr/bin/env python3
"""Merge inferred structure layouts into symbols/structs_auto.tsv.

Usage: merge_structs.py <structs.tsv> <structs_auto.tsv> <module.tsv>...

Inputs are InferStructs.java outputs, in priority order (A2MENU.DAT first: its debug
info types the most variables). Rows already in structs_auto.tsv are kept unless a
module output disagrees. A field is dropped where it overlaps a hand-written field in
structs.tsv or an earlier, higher-priority inferred field. For a structure laid out in
structs.tsv, inferred fields only fill gaps inside its size (array indexing into a
structure of unknown size looks like large offsets); "__size" otherwise keeps the maximum.
"""
import os
import re
import sys

SIZES = {'char': 1, 'uchar': 1, 'bool': 1, 'undefined1': 1, 'short': 2, 'ushort': 2,
         'undefined2': 2, 'double': 8, 'longlong': 8, 'ulonglong': 8, 'undefined8': 8}


def size_of(t):
    m = re.match(r'(.*)\[(\d+)\]$', t)
    if m:
        return size_of(m.group(1)) * int(m.group(2))
    return 4 if t.endswith('*') else SIZES.get(t, 4)


def rows(path):
    if not os.path.exists(path):
        return []
    out = []
    for line in open(path, encoding='utf-8'):
        if line.startswith(('#', 'struct\t')) or not line.strip():
            continue
        c = line.rstrip('\n').split('\t')
        out.append((c[0], int(c[1], 16), c[2], c[3]))
    return out


def main(manual, auto, *modules):
    taken = {}  # struct -> list of (off, end)
    extent = {}  # hand-written struct -> its size; inferred fields stay inside it
    for s, off, t, n in rows(manual):
        end = off if n == '__size' else off + size_of(t)
        extent[s] = max(extent.get(s, 0), end)
        if n != '__size':
            taken.setdefault(s, []).append((off, end))
    fields, sizes = {}, {}
    for src in list(modules) + [auto]:
        for s, off, t, n in rows(src):
            if n == '__size':
                if s not in extent:
                    sizes[s] = max(sizes.get(s, 0), off)
                continue
            end = off + size_of(t)
            if s in extent and end > extent[s]:
                continue
            if any(o < end and off < e for o, e in taken.get(s, [])):
                continue
            taken.setdefault(s, []).append((off, end))
            fields[(s, off)] = (t, n)
    with open(auto, 'w', encoding='utf-8') as fh:
        fh.write('# Structure fields inferred by tools/InferStructs.java (merged by\n'
                 '# tools/merge_structs.py). Generated: name fields in structs.tsv instead,\n'
                 '# which takes precedence over this file.\n')
        fh.write('struct\toffset\ttype\tname\tcomment\n')
        for s in sorted({k[0] for k in fields} | set(sizes)):
            for (ss, off), (t, n) in sorted(fields.items()):
                if ss == s:
                    fh.write(f'{s}\t{off:02x}\t{t}\t{n}\t\n')
            if s in sizes:
                fh.write(f'{s}\t{sizes[s]:x}\t-\t__size\t\n')
    print(f'merge_structs: {len(fields)} inferred fields in {len({k[0] for k in fields})} '
          f'structures -> {auto}')


if __name__ == '__main__':
    main(*sys.argv[1:])
