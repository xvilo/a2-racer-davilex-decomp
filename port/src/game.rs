//! State shared by the menu and the race: the loaded resources and INFIO.INI.

use std::path::PathBuf;
use std::rc::Rc;

use crate::menu::infio::Infio;
use crate::res::archive::{NamedArchive, SpriteArchive};

pub struct Game {
    /// The original disc (or a copy of it).
    pub data_dir: PathBuf,
    /// A2MRCS: menu screens, sounds.
    pub menu_files: Rc<NamedArchive>,
    /// A2MRC: buttons, labels, anims.
    pub menu_sprites: Rc<SpriteArchive>,
    /// What the menu and the race tell each other (INFIO.INI).
    pub infio: Infio,
}
