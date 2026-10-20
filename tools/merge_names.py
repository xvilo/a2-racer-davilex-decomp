#!/usr/bin/env python3
"""Merge names files: later files override earlier ones per address.

Usage: merge_names.py <out.tsv> <names.tsv> [<override.tsv> ...]

Missing input files are skipped. Columns follow tools/watcom_names.py; an override
may leave cc/ret/params/dtype empty to keep the recovered values (use params "-" for
an explicit empty parameter list), or fill them in. An
override row with kind 'local' instead names a local variable or parameter:
  address=<function entry>  name=<new name>  dtype=<old name>  params=<new type, optional>
(the old name as printed in decompiled.c; cc/ret unused)
"""
import os
import sys

COLS = ['address', 'kind', 'name', 'cc', 'ret', 'params', 'dtype', 'mangled']


def read(path):
    if not os.path.exists(path):
        return []
    lines = [l.rstrip('\n') for l in open(path) if l.strip() and not l.startswith('#')]
    head = lines[0].split('\t')
    rows = []
    for l in lines[1:]:
        r = dict(zip(head, l.split('\t')))
        rows.append([r.get(c, '') for c in COLS])
    return rows


def main(out, *inputs):
    merged, locals_ = {}, []
    for i, path in enumerate(inputs):
        for r in read(path):
            if r[1] == 'local':
                locals_.append(r)
                continue
            key = (r[0].lower().zfill(8), r[1] == 'data')
            if i > 0 and key in merged:
                base = merged[key]
                # An override keeps recovered types unless it supplies its own.
                r = [r[k] if r[k] or k in (0, 1, 2) else base[k] for k in range(len(COLS))]
            merged[key] = r
    with open(out, 'w') as f:
        f.write('\t'.join(COLS) + '\n')
        for _, r in sorted(merged.items()):
            f.write('\t'.join(r) + '\n')
        for r in locals_:
            f.write('\t'.join(r) + '\n')


if __name__ == '__main__':
    main(*sys.argv[1:])
