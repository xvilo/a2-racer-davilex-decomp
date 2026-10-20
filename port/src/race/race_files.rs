//! The race's files: RCS.IND/RCS.IMG (textures, models, sounds), GAMESPR (sprites) and
//! the track files on the disc (fileio.cpp's xvga_src_fopen in the original).

use std::path::{Path, PathBuf};
use std::rc::Rc;

use crate::res::archive::{find_file_ci, read_file_ci, NamedArchive};
use crate::xvga::inifile::set_ini_file_path;

pub struct RaceFiles {
    pub rcs: Rc<NamedArchive>,
    disc_dir: PathBuf,
}

impl RaceFiles {
    pub fn race_files_open(data_dir: &Path) -> Option<RaceFiles> {
        let Some(rcs) = NamedArchive::open(data_dir, "RCS") else {
            eprintln!("cannot read RCS archive in {}", data_dir.display());
            return None;
        };
        Some(RaceFiles { rcs: Rc::new(rcs), disc_dir: data_dir.to_path_buf() })
    }

    /// A file from RCS by name.
    pub fn race_file(&self, name: &str) -> Option<&[u8]> {
        self.rcs.find(name)
    }

    /// A whole file from the disc directory (the tracks).
    pub fn race_disc_file(&self, name: &str) -> Option<Vec<u8>> {
        read_file_ci(&self.disc_dir, name)
    }

    /// Opens an INI file from the disc directory for read_ini_value_setting; false if
    /// it is not there.
    pub fn race_disc_ini(&self, name: &str) -> bool {
        match find_file_ci(&self.disc_dir, name) {
            Some(p) => {
                set_ini_file_path(p);
                true
            }
            None => false,
        }
    }
}
