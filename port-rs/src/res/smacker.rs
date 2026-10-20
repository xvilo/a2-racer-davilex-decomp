//! Smacker video ("SMK2"), the format of A2INTRO.SMK, A2WIN.SMK and A2EXIT.SMK. The
//! original played them with RAD's SMACKW32.DLL; this is an independent decoder for the
//! parts these files use: 8-bit video in 4x4 blocks with Huffman-coded block types,
//! colours and maps, palette updates, and Huffman/DPCM-compressed audio.

/// Bit reader, least significant bit first.
struct Bits<'a> {
    p: &'a [u8],
    /// In bits.
    pos: usize,
}

impl Bits<'_> {
    fn get_bit(&mut self) -> u32 {
        let v = self.p.get(self.pos >> 3).map(|b| (b >> (self.pos & 7) & 1) as u32).unwrap_or(0);
        self.pos += 1;
        v
    }

    fn get_bits(&mut self, n: u32) -> u32 {
        (0..n).fold(0, |v, i| v | self.get_bit() << i)
    }
}

/// Nodes: a leaf holds a value; a branch holds the index of its "1" child, its "0"
/// child following directly.
#[derive(Clone, Copy, Default)]
struct Node {
    leaf: bool,
    value: u32,
    one: usize,
}

#[derive(Default)]
struct Tree {
    n: Vec<Node>,
}

impl Tree {
    fn build8(&mut self, b: &mut Bits, depth: u32) -> usize {
        let i = self.n.len();
        self.n.push(Node::default());
        if depth > 32 || b.get_bit() == 0 {
            self.n[i] = Node { leaf: true, value: b.get_bits(8), one: 0 };
            return i;
        }
        self.build8(b, depth + 1);
        let one = self.build8(b, depth + 1);
        self.n[i].one = one;
        i
    }

    /// The leaf a code leads to.
    fn walk(&self, b: &mut Bits) -> Option<usize> {
        if self.n.is_empty() {
            return None;
        }
        let mut i = 0;
        while !self.n[i].leaf {
            i = if b.get_bit() != 0 { self.n[i].one } else { i + 1 };
            if i >= self.n.len() {
                return None;
            }
        }
        Some(i)
    }

    fn decode(&self, b: &mut Bits) -> u32 {
        self.walk(b).map(|i| self.n[i].value).unwrap_or(0)
    }

    /// An 8-bit tree as stored in the file: a presence bit, the tree, a closing bit.
    fn read8(b: &mut Bits) -> Tree {
        let mut t = Tree::default();
        if b.get_bit() != 0 {
            t.build8(b, 0);
            b.get_bit();
        }
        t
    }
}

/// The 16-bit trees of the video: leaves are a low and a high byte from two 8-bit
/// trees; three leaves are "recent value" slots, reset each frame.
#[derive(Default)]
struct BigTree {
    t: Tree,
    /// Node indices of the three escape leaves.
    cache_leaf: [Option<usize>; 3],
    cache: [u32; 3],
}

impl BigTree {
    fn build(&mut self, b: &mut Bits, lo: &Tree, hi: &Tree, esc: &[u32; 3], depth: u32) -> usize {
        let i = self.t.n.len();
        self.t.n.push(Node::default());
        if depth > 64 || b.get_bit() == 0 {
            let v = lo.decode(b) | hi.decode(b) << 8;
            self.t.n[i] = Node { leaf: true, value: v, one: 0 };
            if let Some(k) = (0..3).find(|&k| v == esc[k] && self.cache_leaf[k].is_none()) {
                self.cache_leaf[k] = Some(i);
            }
            return i;
        }
        self.build(b, lo, hi, esc, depth + 1);
        let one = self.build(b, lo, hi, esc, depth + 1);
        self.t.n[i].one = one;
        i
    }

    fn read(b: &mut Bits) -> BigTree {
        let mut bt = BigTree::default();
        if b.get_bit() == 0 {
            return bt; // absent: every code is 0
        }
        let lo = Tree::read8(b);
        let hi = Tree::read8(b);
        let esc = [b.get_bits(16), b.get_bits(16), b.get_bits(16)];
        bt.build(b, &lo, &hi, &esc, 0);
        b.get_bit();
        bt
    }

    fn reset(&mut self) {
        self.cache = [0; 3];
    }

    fn decode(&mut self, b: &mut Bits) -> u32 {
        let Some(i) = self.t.walk(b) else { return 0 };
        let mut v = self.t.n[i].value;
        for k in 0..3 {
            if self.cache_leaf[k] == Some(i) {
                v = self.cache[k];
            }
        }
        if v != self.cache[0] {
            self.cache[2] = self.cache[1];
            self.cache[1] = self.cache[0];
            self.cache[0] = v;
        }
        v
    }
}

pub struct Smacker {
    data: Vec<u8>,
    pub width: i32,
    pub height: i32,
    pub frames: i32,
    /// Milliseconds per frame.
    pub frame_ms: f64,
    audio_rate: [u32; 7],
    frame_sizes: usize,
    frame_types: usize,
    /// Offset of frame 0.
    first_frame: usize,
    mmap: BigTree,
    mclr: BigTree,
    full: BigTree,
    ty: BigTree,
    /// Next frame to decode, and its offset.
    current: i32,
    offset: usize,
    /// width * height palette indices.
    pub pixels: Vec<u8>,
    /// 256 RGB triples, 8 bits each.
    pub palette: [u8; 768],
}

fn rd32(p: &[u8]) -> u32 {
    u32::from_le_bytes([p[0], p[1], p[2], p[3]])
}

/// 6-bit palette values to 8 bits as the original player did it (set_smack_palette
/// shifts SMACKW32's 6-bit palette left by 2, so white is 252).
fn pal8(v: u8) -> u8 {
    (v & 63) << 2
}

const BLOCK_RUNS: [usize; 64] = [
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 128, 256, 512, 1024, 2048,
];

/// Audio rate flags.
const SMK_AUDIO_COMPRESSED: u32 = 0x8000_0000;
const SMK_AUDIO_PRESENT: u32 = 0x4000_0000;
const SMK_AUDIO_16BIT: u32 = 0x2000_0000;
const SMK_AUDIO_STEREO: u32 = 0x1000_0000;

impl Smacker {
    pub fn open(data: Vec<u8>) -> Option<Smacker> {
        if data.len() < 104 || &data[..4] != b"SMK2" {
            return None;
        }
        let width = rd32(&data[4..]) as i32;
        let height = rd32(&data[8..]) as i32;
        let frames = rd32(&data[12..]) as i32;
        let rate = rd32(&data[16..]) as i32;
        let frame_ms = if rate > 0 { rate as f64 } else if rate < 0 { -rate as f64 / 100.0 } else { 100.0 };
        let flags = rd32(&data[20..]);
        let mut audio_rate = [0; 7];
        for (i, r) in audio_rate.iter_mut().enumerate() {
            *r = rd32(&data[72 + 4 * i..]);
        }
        let trees_size = rd32(&data[52..]) as usize;
        let ring = (flags & 1) as usize;
        let n = frames.max(0) as usize + ring;
        let pos = 104;
        let table = n * 5;
        if pos + table + trees_size > data.len() || width <= 0 || height <= 0 {
            return None;
        }
        let trees_at = pos + table;
        let (mmap, mclr, full, ty) = {
            let mut b = Bits { p: &data[trees_at..trees_at + trees_size], pos: 0 };
            (BigTree::read(&mut b), BigTree::read(&mut b), BigTree::read(&mut b), BigTree::read(&mut b))
        };
        Some(Smacker {
            width,
            height,
            frames,
            frame_ms,
            audio_rate,
            frame_sizes: pos,
            frame_types: pos + n * 4,
            first_frame: trees_at + trees_size,
            mmap,
            mclr,
            full,
            ty,
            current: 0,
            offset: trees_at + trees_size,
            pixels: vec![0; (width * height) as usize],
            palette: [0; 768],
            data,
        })
    }

    fn frame_size(&self, i: i32) -> usize {
        (rd32(&self.data[self.frame_sizes + i as usize * 4..]) & !3) as usize
    }

    fn decode_palette(&mut self, at: usize, len: usize) {
        let old = self.palette;
        let p = &self.data[at..at + len];
        let mut q = 1;
        let mut i = 0usize;
        while i < 256 && q < len {
            let c = p[q];
            q += 1;
            if c & 0x80 != 0 {
                i += (c & 0x7f) as usize + 1; // keep these entries
            } else if c & 0x40 != 0 {
                let mut n = (c & 0x3f) as usize + 1;
                let mut src = if q < len { q += 1; p[q - 1] as usize } else { 0 };
                while n > 0 && i < 256 && src < 256 {
                    self.palette[i * 3..i * 3 + 3].copy_from_slice(&old[src * 3..src * 3 + 3]);
                    i += 1;
                    src += 1;
                    n -= 1;
                }
            } else {
                if q + 2 > len {
                    break;
                }
                self.palette[i * 3] = pal8(c);
                self.palette[i * 3 + 1] = pal8(p[q]);
                self.palette[i * 3 + 2] = pal8(p[q + 1]);
                q += 2;
                i += 1;
            }
        }
    }

    fn decode_video(&mut self, at: usize, len: usize) {
        let data = std::mem::take(&mut self.data);
        let mut b = Bits { p: &data[at..at + len], pos: 0 };
        let w = self.width as usize;
        let (bw, bh) = (w / 4, self.height as usize / 4);
        let total = bw * bh;
        let mut blk = 0;
        for t in [&mut self.mmap, &mut self.mclr, &mut self.full, &mut self.ty] {
            t.reset();
        }
        while blk < total {
            let ty = self.ty.decode(&mut b);
            let mut run = BLOCK_RUNS[(ty >> 2 & 0x3f) as usize];
            while run > 0 && blk < total {
                let mut out = (blk / bw) * 4 * w + (blk % bw) * 4;
                match ty & 3 {
                    0 => {
                        // Two colours and a 16-bit map.
                        let clr = self.mclr.decode(&mut b);
                        let mut map = self.mmap.decode(&mut b);
                        let (hi, lo) = ((clr >> 8) as u8, clr as u8);
                        for _ in 0..4 {
                            for x in 0..4 {
                                self.pixels[out + x] = if map & 1 != 0 { hi } else { lo };
                                map >>= 1;
                            }
                            out += w;
                        }
                    }
                    1 => {
                        // Full: eight 16-bit codes.
                        for _ in 0..4 {
                            let v = self.full.decode(&mut b);
                            self.pixels[out + 2] = v as u8;
                            self.pixels[out + 3] = (v >> 8) as u8;
                            let v = self.full.decode(&mut b);
                            self.pixels[out] = v as u8;
                            self.pixels[out + 1] = (v >> 8) as u8;
                            out += w;
                        }
                    }
                    2 => {} // unchanged
                    _ => {
                        // Solid colour.
                        for _ in 0..4 {
                            self.pixels[out..out + 4].fill((ty >> 8) as u8);
                            out += w;
                        }
                    }
                }
                run -= 1;
                blk += 1;
            }
        }
        self.data = data;
    }

    /// Decodes the next frame; false at the end or on bad data.
    pub fn next_frame(&mut self) -> bool {
        if self.current >= self.frames {
            return false;
        }
        let size = self.frame_size(self.current);
        let types = self.data[self.frame_types + self.current as usize];
        if self.offset + size > self.data.len() {
            return false;
        }
        let mut p = self.offset;
        let mut left = size;
        if types & 1 != 0 {
            let len = self.data[p] as usize * 4;
            if len > left {
                return false;
            }
            self.decode_palette(p, len);
            p += len;
            left -= len;
        }
        for t in 0..7 {
            if types & (2 << t) != 0 {
                if left < 4 {
                    return false;
                }
                let len = rd32(&self.data[p..]) as usize;
                if len > left {
                    return false;
                }
                p += len;
                left -= len;
            }
        }
        self.decode_video(p, left);
        self.offset += size;
        self.current += 1;
        true
    }

    /// The first audio track, decoded in full as signed 16-bit PCM: the samples
    /// (interleaved), the rate and the channel count; None when silent.
    pub fn decode_audio(&self) -> Option<(Vec<i16>, i32, u8)> {
        let ar = self.audio_rate[0];
        if ar & SMK_AUDIO_PRESENT == 0 {
            return None;
        }
        let bits16 = ar & SMK_AUDIO_16BIT != 0;
        let stereo = ar & SMK_AUDIO_STEREO != 0;
        let mut out = Vec::new();
        let mut offset = self.first_frame;
        for f in 0..self.frames {
            let size = self.frame_size(f);
            let types = self.data[self.frame_types + f as usize];
            if offset + size > self.data.len() {
                break;
            }
            let mut p = offset;
            let mut left = size;
            if types & 1 != 0 {
                let len = self.data[p] as usize * 4;
                p += len;
                left -= len.min(left);
            }
            if types & 2 != 0 && left >= 4 {
                let len = rd32(&self.data[p..]) as usize;
                if len <= left {
                    let chunk = &self.data[p + 4..p + len];
                    if ar & SMK_AUDIO_COMPRESSED != 0 {
                        decode_audio_chunk(chunk, &mut out);
                    } else if bits16 {
                        out.extend(chunk.chunks_exact(2).map(|q| i16::from_le_bytes([q[0], q[1]])));
                    } else {
                        out.extend(chunk.iter().map(|&q| ((q as i32 - 128) * 256) as i16));
                    }
                }
            }
            offset += size;
        }
        Some((out, (ar & 0xff_ffff) as i32, 1 + stereo as u8))
    }
}

fn decode_audio_chunk(p: &[u8], out: &mut Vec<i16>) {
    if p.len() < 4 {
        return;
    }
    let unpacked = rd32(p) as usize;
    let mut b = Bits { p: &p[4..], pos: 0 };
    if b.get_bit() == 0 {
        return;
    }
    let stereo = b.get_bit() as usize;
    let bits16 = b.get_bit() != 0;
    let ntrees = 1 << (stereo + bits16 as usize);
    let t: Vec<Tree> = (0..ntrees).map(|_| Tree::read8(&mut b)).collect();
    let samples = if bits16 { unpacked / 2 } else { unpacked };
    let mut pred = [0i32; 2];
    if bits16 {
        for i in (0..=stereo).rev() {
            let v = b.get_bits(16);
            pred[i] = ((v >> 8) | (v & 0xff) << 8) as u16 as i16 as i32;
        }
        out.extend(pred[..=stereo].iter().map(|&v| v as i16));
        for i in stereo + 1..samples {
            let ch = i & stereo;
            let lo = t[ch * 2].decode(&mut b);
            let hi = t[ch * 2 + 1].decode(&mut b);
            pred[ch] = (pred[ch] as i16).wrapping_add((lo | hi << 8) as u16 as i16) as i32;
            out.push(pred[ch] as i16);
        }
    } else {
        for i in (0..=stereo).rev() {
            pred[i] = b.get_bits(8) as i32;
        }
        out.extend(pred[..=stereo].iter().map(|&v| ((v - 128) * 256) as i16));
        for i in stereo + 1..samples {
            let ch = i & stereo;
            pred[ch] = (pred[ch] + t[ch].decode(&mut b) as u8 as i8 as i32) & 0xff;
            out.push(((pred[ch] - 128) * 256) as i16);
        }
    }
}
