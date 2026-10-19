#!/usr/bin/env python3
"""Carry names from a symbolised Watcom PE to a stripped one built from shared code.

Usage: match_functions.py <src.pe> <src-names.tsv> <dst.pe> <dst-names.tsv>

1. Every named source function (bounded by the next code symbol) becomes a byte
   pattern with relocated dwords and rel32 call/jmp displacements masked out.
2. Patterns that occur exactly once in the target's code name that location.
3. Inside each identical pair, calls at the same offset name the callee, and relocated
   dwords that point at named source symbols name the target address. Only targets whose
   whole masked body is identical propagate further. Repeat until nothing new is found.

The output has names.tsv's columns plus 'evidence' (pattern | call | reference).
"""
import re
import struct
import sys


class PE:
    def __init__(self, path):
        d = self.data = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        n = struct.unpack_from('<H', d, pe + 6)[0]
        opt = pe + 24
        self.base = struct.unpack_from('<I', d, opt + 28)[0]
        o = opt + struct.unpack_from('<H', d, pe + 20)[0]
        self.secs = []
        for i in range(n):
            s = d[o + 40 * i:o + 40 * i + 40]
            name = s[:8].rstrip(b'\0').decode()
            va, rsz, rptr = struct.unpack_from('<I', s, 12)[0], *struct.unpack_from('<II', s, 16)
            self.secs.append((name, self.base + va, rsz, rptr))
        self.relocs = set()
        for name, va, rsz, rptr in self.secs:
            if name == '.reloc':
                p, end = rptr, rptr + rsz
                while p + 8 <= end:
                    page, size = struct.unpack_from('<II', d, p)
                    if size < 8:
                        break
                    for k in range((size - 8) // 2):
                        e = struct.unpack_from('<H', d, p + 8 + 2 * k)[0]
                        if e >> 12 == 3:
                            self.relocs.add(self.base + page + (e & 0xfff))
                    p += size
        self.code = next(s for s in self.secs if s[0] in ('BEGTEXT', '.text', 'AUTO', 'CODE'))

    def off(self, va):
        for _, sva, rsz, rptr in self.secs:
            if sva <= va < sva + rsz:
                return rptr + va - sva
        return None

    def read(self, va, n):
        o = self.off(va)
        return self.data[o:o + n] if o is not None else b''

    def dword(self, va):
        b = self.read(va, 4)
        return struct.unpack('<I', b)[0] if len(b) == 4 else None

    def in_code(self, va):
        _, sva, rsz, _ = self.code
        return sva <= va < sva + rsz


def load_names(path):
    rows = [l.rstrip('\n').split('\t') for l in open(path)]
    return rows[0], rows[1:]


def main(src_path, src_names, dst_path, dst_out):
    src, dst = PE(src_path), PE(dst_path)
    header, rows = load_names(src_names)
    by_addr = {}
    for r in rows:
        by_addr.setdefault(int(r[0], 16), r)  # first name at an address is primary
    code_starts = sorted(a for a, r in by_addr.items() if src.in_code(a))
    funcs = {a: r for a, r in by_addr.items() if r[1] == 'function'}
    ends = {a: (code_starts[i + 1] if i + 1 < len(code_starts) else src.code[1] + src.code[2])
            for i, a in enumerate(code_starts)}

    def masked(a):
        """Pattern bytes and the list of (offset, kind) of masked fields."""
        body = bytearray(src.read(a, ends[a] - a))
        mask = [False] * len(body)
        fields = []
        for i in range(len(body)):
            if a + i in src.relocs:
                for k in range(4):
                    if i + k < len(body):
                        mask[i + k] = True
                fields.append((i, 'abs'))
        for i in range(len(body) - 4):
            if body[i] in (0xE8, 0xE9) and not any(mask[i:i + 5]):
                tgt = (a + i + 5 + struct.unpack_from('<i', body, i + 1)[0]) & 0xffffffff
                if tgt in by_addr:
                    for k in range(1, 5):
                        mask[i + k] = True
                    fields.append((i + 1, 'rel'))
        # Trailing alignment padding differs between links.
        while body and not mask[-1] and body[-1] in (0x00, 0x90, 0xCC):
            body.pop()
            mask.pop()
        return bytes(body), mask, fields

    dst_code = dst.read(dst.code[1], dst.code[2])
    found = {}      # dst VA -> (src VA, evidence)
    taken = set()   # src VAs already placed
    conflicts = []  # (dst VA, placed src, contradicting src, evidence)

    def agrees(sva, dva, prefix=None):
        """Unmasked bytes of the source function equal the target's (first `prefix` only)."""
        body, mask, _ = masked(sva)
        n = len(body) if prefix is None else min(prefix, len(body))
        got = dst.read(dva, n)
        return len(got) == n and all(m or b == g for b, g, m in zip(body[:n], got, mask[:n]))

    def place(dva, sva, why):
        if dva in found:
            if found[dva][0] != sva and by_addr[found[dva][0]][2] != by_addr[sva][2]:
                conflicts.append((dva, found[dva][0], sva, why))
            return False
        if sva in taken:
            return False
        found[dva] = (sva, why)
        taken.add(sva)
        return True

    # 1. unique whole-function pattern matches
    patterns = {}
    for a in funcs:
        body, mask, fields = masked(a)
        fixed = sum(1 for m in mask if not m)
        if len(body) < 12 or fixed < 10:
            continue
        rx = b''.join(b'.' if m else re.escape(bytes([c])) for c, m in zip(body, mask))
        hits = [m.start() for m in re.finditer(rx, dst_code, re.DOTALL)]
        if len(hits) == 1:
            patterns[a] = (body, mask, fields)
            place(dst.code[1] + hits[0], a, 'pattern')

    # 2. propagate through calls and absolute references inside matched pairs
    changed = True
    while changed:
        changed = False
        for dva, (sva, _) in list(found.items()):
            if sva not in patterns:
                continue
            _, _, fields = patterns[sva]
            for i, kind in fields:
                if kind == 'rel':
                    st = (sva + i + 4 + struct.unpack('<i', src.read(sva + i, 4))[0]) & 0xffffffff
                    dt = (dva + i + 4 + struct.unpack('<i', dst.read(dva + i, 4))[0]) & 0xffffffff
                    why = 'call'
                else:
                    st, dt = src.dword(sva + i), dst.dword(dva + i)
                    why = 'reference'
                if st in by_addr and dt is not None and dst.off(dt) is not None:
                    is_code = st in funcs or by_addr[st][1] == 'label'
                    if is_code and not dst.in_code(dt):
                        continue
                    if place(dt, st, why):
                        changed = True
                        # Only identical bodies are trusted to carry their own calls further.
                        if st in funcs and st not in patterns and agrees(st, dt):
                            patterns[st] = masked(st)

    with open(dst_out, 'w') as f:
        f.write('\t'.join(header + ['evidence']) + '\n')
        for dva in sorted(found):
            sva, why = found[dva]
            r = list(by_addr[sva])
            if r[1] in ('function', 'label') and not dst.in_code(dva):
                continue
            r[0] = f'{dva:08x}'
            f.write('\t'.join(r + [why]) + '\n')
    from collections import Counter
    kinds = Counter((by_addr[s][1], w) for s, w in found.values())
    print(f'{dst_path}: {len(found)} names carried over', dict(kinds))
    print(f'  {len(conflicts)} conflicting placements')
    for dva, a, b, why in conflicts[:15]:
        print(f'    {dva:08x}: {by_addr[a][2]} ({found[dva][1]}) vs {by_addr[b][2]} ({why})')


if __name__ == '__main__':
    main(*sys.argv[1:5])
