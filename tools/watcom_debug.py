#!/usr/bin/env python3
"""Dump the Watcom v3 debug info (modules + global symbols) appended to a PE.

Usage: watcom_debug.py <pe-file> <out-dir>
Writes modules.tsv and globals.tsv (address is the absolute VA, using the
PE section with the matching 1-based segment number).
"""
import os
import struct
import sys


def pe_sections(d):
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = pe + 24
    image_base = struct.unpack_from('<I', d, opt + 28)[0]
    o = opt + struct.unpack_from('<H', d, pe + 20)[0]
    secs = []
    for i in range(n):
        s = d[o + 40 * i:o + 40 * i + 40]
        secs.append((s[:8].rstrip(b'\0').decode(), image_base + struct.unpack_from('<I', s, 12)[0]))
    return secs


def main(path, out):
    d = open(path, 'rb').read()
    sig, _, _, _, _, lang_size, seg_size, dsize = struct.unpack_from('<HBBBBHHI', d, len(d) - 14)
    if sig != 0x8386:
        sys.exit(f'{path}: no Watcom debug info')
    base = len(d) - dsize
    langs = d[base:base + lang_size]
    sec = base + lang_size + seg_size
    mod_off, gbl_off, addr_off, _, _ = struct.unpack_from('<IIIIH', d, sec)
    secs = pe_sections(d)

    modules = []
    p = sec + mod_off
    while p < sec + gbl_off:
        lang = struct.unpack_from('<H', d, p)[0]
        n = d[p + 20]
        name = d[p + 21:p + 21 + n].decode('latin-1')
        modules.append((langs[lang:].split(b'\0')[0].decode(), name))
        p += 21 + n

    globs = []
    p = sec + gbl_off
    while p < sec + addr_off:
        off, seg, mod, kind, n = struct.unpack_from('<IHHBB', d, p)
        name = d[p + 10:p + 10 + n].decode('latin-1')
        va = secs[seg - 1][1] + off
        globs.append((va, secs[seg - 1][0], mod, kind, name))
        p += 10 + n

    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'modules.tsv'), 'w') as f:
        f.write('index\tlanguage\tsource\n')
        for i, (lang, name) in enumerate(modules):
            f.write(f'{i}\t{lang}\t{name}\n')
    with open(os.path.join(out, 'globals.tsv'), 'w') as f:
        f.write('address\tsection\tmodule\tkind\tname\n')
        for va, secname, mod, kind, name in sorted(globs):
            f.write(f'{va:08x}\t{secname}\t{mod}\t{kind}\t{name}\n')
    print(f'{len(modules)} modules, {len(globs)} globals')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
