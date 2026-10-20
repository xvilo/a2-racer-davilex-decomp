//! Reading the game straight from its CD image: a raw .bin (2352-byte sectors, Mode 1
//! or Mode 2 XA) or an .iso (2048-byte sectors). The files in the disc's root are
//! unpacked once into a folder, which then serves as the data folder.

use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

const SECTOR: usize = 2048;
const SYNC: [u8; 12] = [0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0];

struct Disc {
    f: File,
    /// 2352-byte sectors.
    raw: bool,
}

impl Disc {
    /// The 2048 user bytes of a sector.
    fn read_sector(&mut self, lba: usize, out: &mut [u8; SECTOR]) -> std::io::Result<()> {
        if !self.raw {
            self.f.seek(SeekFrom::Start((lba * SECTOR) as u64))?;
            return self.f.read_exact(out);
        }
        let mut s = [0u8; 2352];
        self.f.seek(SeekFrom::Start((lba * 2352) as u64))?;
        self.f.read_exact(&mut s)?;
        if s[..12] != SYNC {
            out.fill(0);
            return Ok(());
        }
        // Mode 1: data after the 16-byte header; Mode 2 XA form 1: after the subheader too.
        let at = if s[15] == 1 { 16 } else { 24 };
        out.copy_from_slice(&s[at..at + SECTOR]);
        Ok(())
    }

    fn read_extent(&mut self, mut lba: usize, size: usize) -> std::io::Result<Vec<u8>> {
        let mut out = Vec::with_capacity(size);
        let mut sector = [0u8; SECTOR];
        while out.len() < size {
            self.read_sector(lba, &mut sector)?;
            let n = (size - out.len()).min(SECTOR);
            out.extend_from_slice(&sector[..n]);
            lba += 1;
        }
        Ok(out)
    }
}

/// Whether the path names a CD image (by its extension).
pub fn is_disc_image(path: &Path) -> bool {
    path.extension()
        .map(|e| ["bin", "iso", "img"].iter().any(|x| e.eq_ignore_ascii_case(x)))
        .unwrap_or(false)
}

fn rd32(p: &[u8]) -> usize {
    u32::from_le_bytes([p[0], p[1], p[2], p[3]]) as usize
}

/// Unpacks the root files of the image into out_dir (created if needed). Does nothing
/// when out_dir already holds the game.
pub fn disc_image_unpack(image: &Path, out_dir: &Path) -> Result<(), String> {
    if out_dir.join("A2MENU.DAT").is_file() {
        return Ok(());
    }
    let err = |e: std::io::Error| format!("{}: {e}", image.display());
    let mut f = File::open(image).map_err(err)?;
    let mut head = [0u8; 12];
    let raw = f.read_exact(&mut head).is_ok() && head == SYNC;
    let mut d = Disc { f, raw };
    // The primary volume descriptor at sector 16: ISO 9660 names, upper case "NAME;1".
    let mut pvd = [0u8; SECTOR];
    if d.read_sector(16, &mut pvd).is_err() || pvd[0] != 1 || &pvd[1..6] != b"CD001" {
        return Err(format!("{}: not an ISO 9660 CD image", image.display()));
    }
    let root = &pvd[156..190];
    let dir = d.read_extent(rd32(&root[2..]), rd32(&root[10..])).map_err(err)?;
    fs::create_dir_all(out_dir).map_err(|e| format!("{}: {e}", out_dir.display()))?;
    eprintln!("unpacking {} into {} (once)", image.display(), out_dir.display());
    let mut i = 0;
    let mut files = 0;
    while i < dir.len() {
        let len = dir[i] as usize;
        if len == 0 {
            i = (i / SECTOR + 1) * SECTOR; // records don't cross sectors
            continue;
        }
        let r = &dir[i..(i + len).min(dir.len())];
        i += len;
        let name_len = r[32] as usize;
        if r[25] & 2 != 0 || name_len == 0 || (name_len == 1 && r[33] <= 1) {
            continue; // directories, "." and ".."
        }
        let mut name = String::from_utf8_lossy(&r[33..33 + name_len]).into_owned();
        if let Some(semi) = name.find(';') {
            name.truncate(semi);
        }
        if name.ends_with('.') {
            name.pop();
        }
        let path = out_dir.join(&name);
        let data = d.read_extent(rd32(&r[2..]), rd32(&r[10..])).map_err(err)?;
        if let Err(e) = fs::write(&path, data) {
            let _ = fs::remove_file(out_dir.join("A2MENU.DAT"));
            return Err(format!("cannot unpack {name}: {e}"));
        }
        files += 1;
    }
    eprintln!("unpacked {files} files");
    Ok(())
}
