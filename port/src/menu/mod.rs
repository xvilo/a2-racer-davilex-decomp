//! The menus (A2MENU.DAT). Their state lives in `Menu`; each original source file is a
//! module adding its functions to it, under the original's names.

pub mod a2types;
pub mod forms;
pub mod help;
pub mod infio;
pub mod m_bedien;
pub mod m_clas;
pub mod m_main;
pub mod m_nsplr;
pub mod m_opties;
pub mod m_score;
pub mod m_stand;
pub mod m_start;
pub mod menu_data;
pub mod menu_types;
pub mod menu_util;
pub mod mysound;
pub mod optie;
pub mod requesters;
pub mod score;
pub mod slot;
pub mod spel;
pub mod txttool;

use a2types::{Bedien, Opties, Slot, SLOT_COUNT};
use menu_data::{menu_data, MenuData};
use menu_types::SingleScore;

pub use score::SCORE_COUNT;

pub struct Menu {
    pub md: MenuData,
    // optie.cpp, bedien.cpp
    pub opties: Opties,
    pub control: Bedien,
    pub available_joy: bool,
    // slot.cpp
    pub slot_list: [Slot; SLOT_COUNT],
    pub active_slot: i32,
    slots_already_loaded: bool,
    // score.cpp: score_t as SCORE.A2R holds it
    pub hiscore: [SingleScore; SCORE_COUNT],
    pub levelscore: [[SingleScore; SCORE_COUNT]; 6],
    // mysound.cpp: 1 when there is no sound device
    pub mysnd_state: i32,
    // help.cpp
    help_keys: Vec<i32>,
    // forms.cpp
    pub forms: forms::Forms,
    // m_main.cpp
    mm_tune_buffer: i32,
}

impl Menu {
    pub fn new() -> Menu {
        let md = menu_data();
        let empty = md.score_list[0];
        Menu {
            md,
            opties: Opties::default(),
            control: Bedien::default(),
            available_joy: false,
            slot_list: [Slot::default(); SLOT_COUNT],
            active_slot: 0,
            slots_already_loaded: false,
            hiscore: [empty; SCORE_COUNT],
            levelscore: [[empty; SCORE_COUNT]; 6],
            mysnd_state: 0,
            help_keys: Vec::new(),
            forms: forms::Forms::new(),
            mm_tune_buffer: -1,
        }
    }

    /// The sprite of frame `frame` of anim `anim` (anims[anim].frames[frame].image).
    pub fn anim_image(&self, anim: i32, frame: i32) -> i32 {
        let a = &self.md.anims[anim as usize];
        let (table, first) = a.frames;
        self.md.frames(table)[first + frame as usize].image
    }
}
