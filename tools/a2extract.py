#!/usr/bin/env python3
"""Unpack A2 Racer's resource archives and convert their images to PNG.

Usage: a2extract.py <disc-dir> <out-dir> [--palette FILE.PCX]

Archives on the disc come in pairs, <name>.IND (index) and <name>.IMG (data):

  named   (RCS, A2MRCS)   IND: u16 count, then count x 24-byte entries
                               { char name[16]; u32 unused; u32 offset into IMG }
                          An entry runs to the next entry's offset (or the end).
  sprites (A2MRC, GAMESPR) IND: u16 count, then count x u32 offset into IMG
                          IMG: u16 count, then per sprite
                               { u16 width, height, center_x, center_y; u8 pixels[w*h] }
                          Pixels are 8-bit palette indices; the palette comes from a
                          PCX picture (the menu uses its screen's background).

Named archives are written out as files (PCX, WAV, D3D); every PCX also gets a PNG.
Sprites become PNGs using --palette (default: hoofd.pcx from A2MRCS for A2MRC, the
first PCX in RCS for GAMESPR). The output is for inspection and must not be committed:
it is the game's copyrighted data.
"""
import os
import struct
import sys
import zlib

NAMED = ('RCS', 'A2MRCS')
SPRITES = {'A2MRC': ('A2MRCS', 'hoofd.pcx'), 'GAMESPR': ('RCS', None)}


def read_named(disc, name):
    ind = open(os.path.join(disc, name + '.IND'), 'rb').read()
    img = open(os.path.join(disc, name + '.IMG'), 'rb').read()
    n = struct.unpack_from('<H', ind)[0]
    entries = []
    for i in range(n):
        o = 2 + 24 * i
        fname = ind[o:o + 16].split(b'\0')[0].decode('latin1')
        off = struct.unpack_from('<I', ind, o + 20)[0]
        entries.append((fname, off))
    files = {}
    ends = sorted({off for _, off in entries} | {len(img)})
    for fname, off in entries:
        end = next(e for e in ends if e > off)
        files[fname] = img[off:end]
    return files


def read_sprites(disc, name):
    ind = open(os.path.join(disc, name + '.IND'), 'rb').read()
    img = open(os.path.join(disc, name + '.IMG'), 'rb').read()
    n = struct.unpack_from('<H', ind)[0]
    sprites = []
    for i in range(n):
        off = struct.unpack_from('<I', ind, 2 + 4 * i)[0]
        w, h, cx, cy = struct.unpack_from('<4H', img, off)
        sprites.append((w, h, cx, cy, img[off + 8:off + 8 + w * h]))
    return sprites


def decode_pcx(data):
    """8-bit RLE PCX with a 256-colour palette at the end -> (w, h, pixels, palette)."""
    (manuf, version, enc, bpp, x0, y0, x1, y1) = struct.unpack_from('<4B4H', data)
    planes = data[65]
    bpl = struct.unpack_from('<H', data, 66)[0]
    if manuf != 10 or bpp != 8 or planes != 1:
        raise ValueError('unsupported PCX (manuf %d, bpp %d, planes %d)' % (manuf, bpp, planes))
    w, h = x1 - x0 + 1, y1 - y0 + 1
    out = bytearray()
    i = 128
    need = bpl * h
    while len(out) < need and i < len(data):
        b = data[i]
        i += 1
        if enc == 1 and b >= 0xC0:
            out += bytes([data[i]]) * (b & 0x3F)
            i += 1
        else:
            out.append(b)
    pixels = b''.join(out[y * bpl:y * bpl + w] for y in range(h))
    palette = data[-768:] if data[-769] == 12 else bytes(range(256)) * 3
    return w, h, pixels, palette


def write_png(path, w, h, pixels, palette):
    """Indexed-colour PNG; palette index 0 is left opaque."""
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body))
    raw = b''.join(b'\0' + pixels[y * w:(y + 1) * w] for y in range(h))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
    png += chunk(b'PLTE', bytes(palette[:768])) + chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def main(disc, out, palette_file=None):
    named = {}
    for name in NAMED:
        files = read_named(disc, name)
        named[name] = files
        d = os.path.join(out, name)
        os.makedirs(d, exist_ok=True)
        for fname, data in files.items():
            with open(os.path.join(d, fname), 'wb') as f:
                f.write(data)
            if fname.lower().endswith('.pcx'):
                try:
                    w, h, px, pal = decode_pcx(data)
                    write_png(os.path.join(d, fname[:-4] + '.png'), w, h, px, pal)
                except ValueError as e:
                    print(f'{name}/{fname}: {e}')
        print(f'{name}: {len(files)} files -> {d}')
    for name, (arch, pcx) in SPRITES.items():
        if palette_file:
            pal = decode_pcx(open(palette_file, 'rb').read())[3]
        else:
            pcxs = [k for k in named[arch] if k.lower().endswith('.pcx')]
            key = next((k for k in pcxs if pcx and k.lower() == pcx), pcxs[0])
            pal = decode_pcx(named[arch][key])[3]
        d = os.path.join(out, name)
        os.makedirs(d, exist_ok=True)
        sprites = read_sprites(disc, name)
        with open(os.path.join(d, 'sprites.tsv'), 'w') as idx:
            idx.write('index\twidth\theight\tcenter_x\tcenter_y\n')
            for i, (w, h, cx, cy, px) in enumerate(sprites):
                idx.write(f'{i}\t{w}\t{h}\t{cx}\t{cy}\n')
                if w and h:
                    write_png(os.path.join(d, f'{i:03d}.png'), w, h, px, pal)
        print(f'{name}: {len(sprites)} sprites -> {d}')


if __name__ == '__main__':
    args = sys.argv[1:]
    pal = None
    if '--palette' in args:
        k = args.index('--palette')
        pal = args[k + 1]
        del args[k:k + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    main(args[0], args[1], pal)
