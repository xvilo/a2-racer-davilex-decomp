//! Minimal PNG writer for screenshots (indexed or RGB colour, uncompressed deflate).

use std::fs;
use std::io;
use std::path::Path;

fn crc32(parts: &[&[u8]]) -> u32 {
    let mut crc = !0u32;
    for p in parts {
        for &b in *p {
            crc ^= b as u32;
            for _ in 0..8 {
                crc = if crc & 1 != 0 { 0xedb8_8320 ^ (crc >> 1) } else { crc >> 1 };
            }
        }
    }
    !crc
}

fn chunk(out: &mut Vec<u8>, tag: &[u8; 4], body: &[u8]) {
    out.extend_from_slice(&(body.len() as u32).to_be_bytes());
    out.extend_from_slice(tag);
    out.extend_from_slice(body);
    out.extend_from_slice(&crc32(&[tag, body]).to_be_bytes());
}

/// `bpp` bytes per pixel: 1 indexed (with palette) or 3 RGB.
fn png_write(path: &Path, pixels: &[u8], w: usize, h: usize, bpp: usize, palette: Option<&[u8]>) -> io::Result<()> {
    let mut out = vec![0x89, b'P', b'N', b'G', b'\r', b'\n', 0x1a, b'\n'];
    let mut ihdr = Vec::with_capacity(13);
    ihdr.extend_from_slice(&(w as u32).to_be_bytes());
    ihdr.extend_from_slice(&(h as u32).to_be_bytes());
    // Bit depth 8; indexed colour or RGB.
    ihdr.extend_from_slice(&[8, if bpp == 1 { 3 } else { 2 }, 0, 0, 0]);
    chunk(&mut out, b"IHDR", &ihdr);
    if let Some(pal) = palette {
        chunk(&mut out, b"PLTE", &pal[..768]);
    }
    // The rows with their filter byte, in a zlib stream of stored deflate blocks.
    let stride = w * bpp;
    let mut raw = Vec::with_capacity((stride + 1) * h);
    for row in pixels.chunks(stride).take(h) {
        raw.push(0);
        raw.extend_from_slice(row);
    }
    let mut z = vec![0x78, 0x01];
    let blocks: Vec<&[u8]> = raw.chunks(65535).collect();
    for (i, b) in blocks.iter().enumerate() {
        let n = b.len() as u16;
        z.push((i + 1 == blocks.len()) as u8);
        z.extend_from_slice(&n.to_le_bytes());
        z.extend_from_slice(&(!n).to_le_bytes());
        z.extend_from_slice(b);
    }
    let (mut a, mut s) = (1u32, 0u32);
    for &v in &raw {
        a = (a + v as u32) % 65521;
        s = (s + a) % 65521;
    }
    z.extend_from_slice(&(s << 16 | a).to_be_bytes());
    chunk(&mut out, b"IDAT", &z);
    chunk(&mut out, b"IEND", &[]);
    fs::write(path, out)
}

pub fn png_write_indexed(path: &Path, pixels: &[u8], w: usize, h: usize, palette: &[u8]) -> io::Result<()> {
    png_write(path, pixels, w, h, 1, Some(palette))
}

pub fn png_write_rgb(path: &Path, rgb: &[u8], w: usize, h: usize) -> io::Result<()> {
    png_write(path, rgb, w, h, 3, None)
}
