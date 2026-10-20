//! 8-bit run-length PCX pictures with a 256-colour palette (rcpcx.cpp in the original).

pub struct PcxImage {
    pub width: i32,
    pub height: i32,
    /// width * height palette indices.
    pub pixels: Vec<u8>,
    /// RGB, 8 bits per channel.
    pub palette: [u8; 768],
}

pub fn pcx_decode(data: &[u8]) -> Option<PcxImage> {
    let size = data.len();
    if size < 128 + 769 || data[0] != 10 || data[3] != 8 || data[65] != 1 {
        return None;
    }
    let word = |i: usize| data[i] as i32 | (data[i + 1] as i32) << 8;
    let (x0, y0, x1, y1, bpl) = (word(4), word(6), word(8), word(10), word(66));
    let (w, h) = (x1 - x0 + 1, y1 - y0 + 1);
    if w <= 0 || h <= 0 || bpl < w {
        return None;
    }
    let (w, h, bpl) = (w as usize, h as usize, bpl as usize);
    let mut line = vec![0u8; bpl];
    let mut pixels = vec![0u8; w * h];
    let (mut i, end) = (128, size - 769);
    for y in 0..h {
        let mut n = 0;
        while n < bpl && i < end {
            let b = data[i];
            i += 1;
            if data[2] == 1 && b >= 0xc0 {
                let run = (b & 0x3f) as usize;
                let v = if i < end { i += 1; data[i - 1] } else { 0 };
                for _ in 0..run.min(bpl - n) {
                    line[n] = v;
                    n += 1;
                }
            } else {
                line[n] = b;
                n += 1;
            }
        }
        line[n..].fill(0);
        pixels[y * w..(y + 1) * w].copy_from_slice(&line[..w]);
    }
    let mut palette = [0u8; 768];
    if data[size - 769] == 12 {
        palette.copy_from_slice(&data[size - 768..]);
    }
    Some(PcxImage { width: w as i32, height: h as i32, pixels, palette })
}
