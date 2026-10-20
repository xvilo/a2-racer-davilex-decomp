#!/usr/bin/env python3
"""Generate the port's menu tables (port/src/menu/menu_data.[ch]) from A2MENU.DAT.

Usage: menu_tables.py <A2MENU.DAT> <symbols/A2MENU.DAT.tsv> <out-dir>

The menus are driven by static tables in A2MENU.DAT's data section, named by its debug
info: anims (anim_t: frame count, mode, frame list), frame lists (frame_t: duration,
sprite), forms (form_t: one row per control), buttons (button_t) and help texts
(help_t). Each table runs up to the next symbol. Pointers inside them are written back
as references to the table they point into.
"""
import re
import struct
import sys

LAYOUTS = {
    # type: (size, struct format, field names)
    'anim_t': (12, '<iiI', ('frame_count', 'mode', 'frames')),
    'frame_t': (8, '<ii', ('duration', 'image')),
    'form_t': (28, '<iiiIiiI', ('x', 'y', 'type', 'control', 'state', 'field_14', 'hwnd')),
    'button_t': (21, '<iiibii', ('image', 'width', 'height', 'hotkey', 'focus_anim', 'focus_frame')),
    'help_t': (8, '<iI', ('control_id', 'text')),
    'areabutton_t': (17, '<iiiib', ('width', 'height', 'image', 'field_0c', 'hotkey')),
    'mst_slot_t': (8, '<ii', ('x', 'y')),
    'mc_keyset_t': (8, '<iI', ('vk', 'name')),
    'single_score_t': (44, '<32sihhi', ('name32', 'time', 'stage', 'is_new', 'money')),
    'mc_config_key_t': (24, '<iiiiIi', ('x', 'y', 'x2', 'y2', 'label', 'key')),
    'edit_box_t': (44, '<iiiiIIIIIII', ('width', 'height', 'max_chars', 'lines', 'text',
                                       'background', 'foreground', 'brush', 'focus_background',
                                       'focus_foreground', 'focus_brush')),
    'mns_slot_t': (8, '<ii', ('x', 'y')),
}


def load_pe(path):
    b = open(path, 'rb').read()
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    nsec = struct.unpack_from('<H', b, pe + 6)[0]
    opt = struct.unpack_from('<H', b, pe + 20)[0]
    base = struct.unpack_from('<I', b, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + 40 * i
        vsz, va, rsz, raw = struct.unpack_from('<IIII', b, o + 8)
        secs.append((base + va, max(vsz, rsz), raw, rsz))

    def read(addr, n):
        for va, size, raw, rsz in secs:
            if va <= addr < va + size:
                off = addr - va
                chunk = b[raw + off:raw + min(off + n, rsz)] if off < rsz else b''
                return chunk + b'\0' * (n - len(chunk))
        raise ValueError(hex(addr))
    return read


def c_string(read, addr):
    s = bytearray()
    while len(s) < 4096:
        c = read(addr + len(s), 1)[0]
        if c == 0:
            break
        s.append(c)
    out = ''
    for ch in s.decode('latin1'):
        if ch in '"\\':
            out += '\\' + ch
        elif ch == '\n':
            out += '\\n'
        elif ' ' <= ch <= '~':
            out += ch
        else:
            out += '\\%03o' % ord(ch)
    return '"' + out + '"'


def main(binary, symbols, out):
    read = load_pe(binary)
    rows = []
    for line in open(symbols, encoding='utf-8'):
        c = line.rstrip('\n').split('\t')
        if len(c) > 6 and c[1] in ('data', 'label') and c[0] != 'address':
            rows.append((int(c[0], 16), c[2], c[6], c[7] if len(c) > 7 else ''))
    rows.sort()
    starts = [a for a, _, _, _ in rows]
    tables = []  # (addr, name, type, count)
    scalars = []  # (addr, name, C type, count, inner dimension or None)
    for i, (addr, name, dtype, mangled) in enumerate(rows):
        nxt = starts[i + 1] if i + 1 < len(starts) else addr + 4
        if name == 'score_list':
            # score_t: the overall top ten and six stage top tens, as one table.
            tables.append((addr, name, 'single_score_t', 70))
            continue
        m = re.fullmatch(r'(\w+)\[\]', dtype)
        if m and m.group(1) in LAYOUTS:
            t = m.group(1)
            tables.append((addr, name, t, max(1, (nxt - addr) // LAYOUTS[t][0])))
            continue
        # Arrays of strings and of short/long, e.g. mm_tune (char*[]) and
        # mm_entree_anim_tabel (short[][12]).
        m = re.search(r'\$n\[\](?:\[(\d+)\])?(pna|s|l|i)$', mangled)
        if m:
            ctype, size = {'pna': ('const char *', 4), 's': ('short', 2), 'l': ('long', 4),
                           'i': ('int', 4)}[m.group(2)]
            inner = int(m.group(1)) if m.group(1) else None
            n = (nxt - addr) // (size * (inner or 1))
            if n > 0:
                scalars.append((addr, name, ctype, n, inner))

    def ref(ptr):
        """C expression for a pointer into one of the tables (or NULL)."""
        if ptr == 0:
            return 'NULL'
        for addr, name, t, n in tables:
            size = LAYOUTS[t][0]
            if addr <= ptr < addr + n * size and (ptr - addr) % size == 0:
                return f'&{name}[{(ptr - addr) // size}]'
        return None

    decls, defs = [], []
    for addr, name, t, n in tables:
        size, fmt, fields = LAYOUTS[t]
        decls.append(f'extern {t} {name}[{n}];')
        body = []
        for k in range(n):
            vals = struct.unpack(fmt, read(addr + k * size, size))
            parts = []
            for f, v in zip(fields, vals):
                if f in ('frames', 'control'):
                    r = ref(v)
                    parts.append(r if r else f'NULL /* {v:#x} */')
                elif f == 'name32':
                    raw = v.split(b'\0')[0].decode('latin1').replace('\\', '\\\\').replace('"', '\\"')
                    parts.append('"' + raw + '"')
                elif f in ('text', 'name', 'label'):
                    parts.append(c_string(read, v) if v else 'NULL')
                elif f in ('brush', 'focus_brush'):
                    continue
                elif f == 'hwnd':
                    continue
                elif f == 'hotkey':
                    parts.append(str(v))
                else:
                    parts.append(str(v))
            body.append('\t{ ' + ', '.join(parts) + ' },')
        defs.append(f'/* {addr:#x} */\n{t} {name}[{n}] = {{\n' + '\n'.join(body) + '\n};\n')

    for addr, name, ctype, n, inner in scalars:
        size = {'const char *': 4, 'short': 2, 'long': 4, 'int': 4}[ctype]
        fmt = {'const char *': '<I', 'short': '<h', 'long': '<i', 'int': '<i'}[ctype]
        cell = lambda a: (c_string(read, struct.unpack('<I', read(a, 4))[0])
                          if ctype == 'const char *' else str(struct.unpack(fmt, read(a, size))[0]))
        if inner:
            decls.append(f'extern {ctype} {name}[{n}][{inner}];')
            body = ['\t{ ' + ', '.join(cell(addr + (r * inner + k) * size) for k in range(inner)) + ' },'
                    for r in range(n)]
            defs.append(f'/* {addr:#x} */\n{ctype} {name}[{n}][{inner}] = {{\n' + '\n'.join(body) + '\n};\n')
        else:
            decls.append(f'extern {ctype}{"" if ctype.endswith("*") else " "}{name}[{n}];')
            vals = [cell(addr + k * size) for k in range(n)]
            defs.append(f'/* {addr:#x} */\n{ctype}{"" if ctype.endswith("*") else " "}{name}[{n}] = {{\n\t'
                        + ',\n\t'.join(vals) + ',\n};\n')

    with open(f'{out}/menu_data.h', 'w') as h:
        h.write('/* Menu tables from A2MENU.DAT. Generated by tools/menu_tables.py; do not edit. */\n'
                '#ifndef A2_MENU_DATA_H\n#define A2_MENU_DATA_H\n\n#include "menu/menu_types.h"\n\n')
        h.write('\n'.join(decls) + '\n\n#endif\n')
    with open(f'{out}/menu_data.c', 'w') as c:
        c.write('/* Menu tables from A2MENU.DAT. Generated by tools/menu_tables.py; do not edit. */\n'
                '#include <stddef.h>\n\n#include "menu/menu_data.h"\n\n')
        c.write('\n'.join(defs))
    print(f'menu_tables: {len(tables) + len(scalars)} tables -> {out}/menu_data.[ch]')


if __name__ == '__main__':
    main(*sys.argv[1:])
