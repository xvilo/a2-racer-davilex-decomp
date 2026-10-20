//! Resource archives: `<name>.IND` (index) + `<name>.IMG` (data).
//!
//! Named archives (RCS, A2MRCS) hold files by name: PCX pictures, WAV sounds and D3D
//! models. Sprite archives (A2MRC, GAMESPR) hold numbered 8-bit images with a centre
//! point. The original reads them in fileio.cpp and the mem image code of the xvga
//! library.

use std::fs;
use std::path::{Path, PathBuf};

pub struct ArchiveEntry {
    pub name: String,
    pub offset: usize,
    pub size: usize,
}

#[derive(Default)]
pub struct NamedArchive {
    /// The whole .IMG file.
    pub data: Vec<u8>,
    pub entries: Vec<ArchiveEntry>,
}

#[derive(Default, Clone)]
pub struct Sprite {
    pub width: i32,
    pub height: i32,
    pub center_x: i32,
    pub center_y: i32,
    /// width * height palette indices.
    pub pixels: Vec<u8>,
}

#[derive(Default)]
pub struct SpriteArchive {
    pub sprites: Vec<Sprite>,
}

fn rd16(p: &[u8]) -> usize {
    p[0] as usize | (p[1] as usize) << 8
}

fn rd32(p: &[u8]) -> usize {
    p[0] as usize | (p[1] as usize) << 8 | (p[2] as usize) << 16 | (p[3] as usize) << 24
}

/// The disc is ISO 9660 (upper case); copies on other file systems may differ in case.
pub fn find_file_ci(dir: &Path, name: &str) -> Option<PathBuf> {
    let path = dir.join(name);
    if path.is_file() {
        return Some(path);
    }
    // The Windows file system already ignores case; elsewhere look through the folder.
    fs::read_dir(dir).ok()?.flatten().find_map(|e| {
        let n = e.file_name();
        (n.to_string_lossy().eq_ignore_ascii_case(name)).then(|| e.path())
    })
}

/// Reads a whole file, matching its name in `dir` without regard to case.
pub fn read_file_ci(dir: &Path, name: &str) -> Option<Vec<u8>> {
    fs::read(find_file_ci(dir, name)?).ok()
}

fn load_pair(dir: &Path, name: &str) -> Option<(Vec<u8>, Vec<u8>)> {
    let ind = read_file_ci(dir, &format!("{name}.IND"))?;
    let img = read_file_ci(dir, &format!("{name}.IMG"))?;
    (ind.len() >= 2).then_some((ind, img))
}

impl NamedArchive {
    /// Reads `<dir>/<name>.IND` and `.IMG`.
    pub fn open(dir: &Path, name: &str) -> Option<NamedArchive> {
        let (ind, data) = load_pair(dir, name)?;
        let n = rd16(&ind);
        if ind.len() < 2 + n * 24 {
            return None;
        }
        let mut entries: Vec<ArchiveEntry> = (0..n)
            .map(|i| {
                let e = &ind[2 + 24 * i..2 + 24 * (i + 1)];
                let len = e[..15].iter().position(|&c| c == 0).unwrap_or(15);
                ArchiveEntry {
                    name: String::from_utf8_lossy(&e[..len]).into_owned(),
                    offset: rd32(&e[20..]),
                    size: 0,
                }
            })
            .collect();
        // An entry runs up to the next entry's offset.
        let mut ends: Vec<usize> = entries.iter().map(|e| e.offset).collect();
        ends.push(data.len());
        ends.sort_unstable();
        for e in &mut entries {
            let end = ends.iter().copied().find(|&x| x > e.offset).unwrap_or(data.len());
            e.size = end.saturating_sub(e.offset);
        }
        Some(NamedArchive { data, entries })
    }

    /// Case-insensitive lookup.
    pub fn find(&self, file: &str) -> Option<&[u8]> {
        let e = self.entries.iter().find(|e| e.name.eq_ignore_ascii_case(file))?;
        self.data.get(e.offset..e.offset + e.size)
    }
}

impl SpriteArchive {
    pub fn open(dir: &Path, name: &str) -> Option<SpriteArchive> {
        let (ind, data) = load_pair(dir, name)?;
        let n = rd16(&ind);
        if ind.len() < 2 + n * 4 {
            return None;
        }
        let sprites = (0..n)
            .map(|i| {
                let off = rd32(&ind[2 + 4 * i..]);
                if off + 8 > data.len() {
                    return Sprite::default();
                }
                let p = &data[off..];
                let (w, h) = (rd16(p), rd16(&p[2..]));
                if off + 8 + w * h > data.len() {
                    return Sprite::default();
                }
                Sprite {
                    width: w as i32,
                    height: h as i32,
                    center_x: rd16(&p[4..]) as i32,
                    center_y: rd16(&p[6..]) as i32,
                    pixels: p[8..8 + w * h].to_vec(),
                }
            })
            .collect();
        Some(SpriteArchive { sprites })
    }
}
