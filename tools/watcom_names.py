#!/usr/bin/env python3
"""Demangle Watcom symbols from watcom_debug.py's globals.tsv into names.tsv.

Usage: watcom_names.py <globals.tsv> <names.tsv>

Output columns (tab-separated):
  address   absolute VA (hex)
  kind      function | label | data
  name      C identifier to apply
  cc        __watcall | __cdecl | __stdcall | "" (unknown)
  ret       return type, "" if unknown (types are "base" plus one "*" per pointer level)
  params    "|"-separated parameter types, "" if none or unknown, "?" if unknown
  dtype     data type for kind=data, "" if unknown
  mangled   original symbol

Encoding rules follow Open Watcom's bld/lib_misc/c/demangle.c.
"""
import re
import sys

BASIC = {'a': 'char', 'b': 'float', 'c': 'char', 'd': 'double', 'e': '...', 'i': 'int',
         'l': 'long', 'q': 'bool', 's': 'short', 't': 'long double', 'v': 'void',
         'w': 'wchar_t', 'z': 'longlong', '_': ''}


class Parser:
    def __init__(self, s, types=None):
        self.s, self.i = s, 0
        self.types = types if types is not None else []  # back-reference table

    def peek(self):
        return self.s[self.i] if self.i < len(self.s) else ''

    def take(self):
        c = self.peek()
        self.i += 1
        return c

    def type_name(self):
        """'$name$' or back-reference '$<digit>$'; consumes the scope terminator '$'."""
        assert self.take() == '$'
        j = self.s.index('$', self.i)
        name = self.s[self.i:j]
        self.i = j + 1
        if name.isdigit():
            return self.types[int(name)]
        if self.peek() == '$':
            self.i += 1
        self.types.append(name)
        return name

    def type(self):
        """Returns (base, pointer_depth, is_function, array)."""
        unsigned = False
        while True:
            c = self.peek()
            if c in 'nfxyh':           # __near, __far, const, volatile, __huge
                self.i += 1
            elif c == 'u':
                unsigned = True
                self.i += 1
            else:
                break
        c = self.peek()
        if c in 'pr':
            self.i += 1
            base, depth, fn, _ = self.type()
            return base, depth + 1, fn, None
        if c == '[':
            j = self.s.index(']', self.i)
            dim = self.s[self.i + 1:j]
            self.i = j + 1
            base, depth, fn, _ = self.type()
            return base, depth, fn, dim
        if c == '(':
            self.i += 1
            while self.peek() != ')':
                self.type()
            self.i += 1
            self.type()  # return type
            return 'void', 0, True, None
        if c == '$':
            return self.type_name(), 0, False, None
        self.i += 1
        base = BASIC[c]
        if unsigned:
            base = 'u' + base if base in ('char', 'short', 'int', 'long', 'longlong') else base
        return base, 0, False, None


def clean_type_name(name, known_words):
    """Watcom names anonymous typedef'd structs '__' + hash + typedef name."""
    if not name.startswith('__') or name in ('__iobuf',):
        return name
    for k in range(5, 8):
        rest = name[2 + k:]
        if rest and rest[0].isalpha() and rest.split('_')[0] in known_words:
            return rest
    return name[8:] if len(name) > 8 else name


def fmt(t, known):
    base, depth, fn, _ = t
    if fn:  # pointer to function: keep as an opaque code pointer
        return 'void' + '*' * max(depth, 1)
    base = clean_type_name(base, known) if base not in BASIC.values() else base
    return base + '*' * depth


def demangle(sym, known):
    """Returns (name, cc, ret, params, dtype, is_func) for one symbol."""
    m = re.match(r'W\?\$Wsi[0-9a-f]{2}([A-Za-z_][A-Za-z0-9_]*)\$n(.*?)\$:\.\d+\$:\?([A-Za-z_][A-Za-z0-9_]*)\$', sym)
    if m:  # function-local static: <var> inside <function>
        p = Parser(m.group(2))
        t = p.type()
        return f'{m.group(3)}__{m.group(1)}', '', '', None, fmt(t, known) + (f'[{t[3]}]' if t[3] is not None else ''), False
    m = re.match(r'W\?([A-Za-z_][A-Za-z0-9_]*)\$n(.*)$', sym)
    if m:
        name, enc = m.groups()
        p = Parser(enc, [name])  # back-reference $0$ is the symbol's own name
        if enc.startswith('('):
            p.i = 1
            params = []
            while p.peek() != ')':
                params.append(p.type())
            p.i += 1
            ret = p.type()
            variadic = any(t[0] == '...' for t in params)
            plist = [fmt(t, known) if t[0] != '...' else '...' for t in params]
            if plist == ['void']:
                plist = []
            return name, '__cdecl' if variadic else '__watcall', fmt(ret, known), plist, '', True
        t = p.type()
        return name, '', '', None, fmt(t, known) + (f'[{t[3]}]' if t[3] is not None else ''), False
    m = re.match(r'_([A-Za-z0-9_]+)@(\d+)$', sym)
    if m:  # stdcall (Win32 API import stubs, WinMain)
        return m.group(1), '__stdcall', '', None, '', True
    if sym.endswith('_') and not sym.startswith('W?'):  # Watcom register-call C function
        return sym.rstrip('_').lstrip('_') or sym, '__watcall', '', None, '', True
    return re.sub(r'\W', '_', sym.lstrip('_')) or sym, '', '', None, '', None


def main(src, dst):
    rows = [l.rstrip('\n').split('\t') for l in open(src)][1:]
    known = set()
    for r in rows:
        # Words of the symbol names themselves (the part before '$'), not of the
        # mangled types, which carry the hash-prefixed names being cleaned.
        for w in re.findall(r'[A-Za-z]+', r[4].split('$')[0].lstrip('W?')):
            known.add(w)
    used = {}
    out = []
    for addr, section, _mod, kind, sym in rows:
        try:
            name, cc, ret, params, dtype, is_func = demangle(sym, known)
        except (IndexError, KeyError, ValueError, AssertionError):
            name, cc, ret, params, dtype, is_func = re.sub(r'\W', '_', sym), '', '', None, '', None
        code = section == 'BEGTEXT'
        if not code:
            k = 'data'
        elif is_func and (kind in ('4', '0') or sym.startswith('W?') or sym.endswith('_') or '@' in sym):
            k = 'function'
        else:
            k = 'label'
        if name in used:  # Ghidra needs unique global names
            used[name] += 1
            name = f'{name}_{used[name]}'
        else:
            used[name] = 0
        out.append((addr, k, name, cc, ret, '?' if params is None else '|'.join(params), dtype, sym))
    with open(dst, 'w') as f:
        f.write('address\tkind\tname\tcc\tret\tparams\tdtype\tmangled\n')
        for r in out:
            f.write('\t'.join(r) + '\n')
    from collections import Counter
    print(Counter(r[1] for r in out), 'typed functions:', sum(1 for r in out if r[4]))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
