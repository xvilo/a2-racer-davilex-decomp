//! Save files (fileio.cpp's file_exist/file_read/file_write/file_delete).
//!
//! The original kept OPTIE.A2R, BEDIEN.A2R, SLOT.A2R, the score files and INFIO.INI in
//! the game directory. The port keeps the same files, byte for byte, in the user's
//! save directory (SDL's pref path), so saves from the original can be copied over.

use std::fs;
use std::path::PathBuf;
use std::sync::RwLock;

static SAVE_DIR: RwLock<PathBuf> = RwLock::new(PathBuf::new());

/// Sets the save directory (created if needed); None picks the platform default.
pub fn fileio_init(dir: Option<PathBuf>) {
    let dir = dir.unwrap_or_else(|| {
        sdl2::filesystem::pref_path("Davilex", "A2 Racer").map(PathBuf::from).unwrap_or_else(|_| PathBuf::from("."))
    });
    let _ = fs::create_dir_all(&dir);
    *SAVE_DIR.write().unwrap() = dir;
}

pub fn fileio_save_dir() -> PathBuf {
    SAVE_DIR.read().unwrap().clone()
}

/// Full path of a save file.
pub fn save_path(name: &str) -> PathBuf {
    SAVE_DIR.read().unwrap().join(name)
}

pub fn file_exist(name: &str) -> bool {
    save_path(name).is_file()
}

/// Exactly `buf.len()` bytes, or None (the original returned nonzero for a missing or
/// short file, after which the caller deletes it and starts over).
pub fn file_read(buf: &mut [u8], name: &str) -> bool {
    match fs::read(save_path(name)) {
        Ok(data) if data.len() >= buf.len() => {
            buf.copy_from_slice(&data[..buf.len()]);
            true
        }
        _ => false,
    }
}

pub fn file_write(buf: &[u8], name: &str) -> bool {
    fs::write(save_path(name), buf).is_ok()
}

pub fn file_delete(name: &str) {
    let _ = fs::remove_file(save_path(name));
}
