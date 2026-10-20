//! Option settings (optie.cpp) and controls (bedien.cpp), kept in OPTIE.A2R and
//! BEDIEN.A2R.

use crate::menu::a2types::{Bedien, Opties, Reader, SOUND_ALL};
use crate::menu::Menu;
use crate::platform::sys::sys_joystick_position;
use crate::xvga::fileio::{file_delete, file_exist, file_read, file_write, save_path};

impl Menu {
    pub fn save_optie(&self) {
        let mut out = Vec::new();
        self.opties.write(&mut out);
        if !file_write(&out, "OPTIE.A2R") {
            eprintln!("save_optie: cannot write {}", save_path("OPTIE.A2R").display());
        }
    }

    pub fn init_optie(&mut self) {
        let mut buf = [0u8; Opties::SIZE];
        if file_exist("OPTIE.A2R") && file_read(&mut buf, "OPTIE.A2R") {
            self.opties = Opties::read(&mut Reader { p: &buf });
            return;
        }
        // A missing or damaged file starts over with the defaults.
        file_delete("OPTIE.A2R");
        self.opties = Opties {
            view: 1,
            resolution: 1,
            sound: SOUND_ALL,
            display_mode: 1,
            horizon: 0,
            dashboard: 0,
            detail: 1,
            camera: 0,
            music_volume: 3,
            effects_volume: 4,
        };
        self.save_optie();
    }

    pub fn save_bedien(&self) {
        let mut out = Vec::new();
        self.control.write(&mut out);
        if !file_write(&out, "BEDIEN.A2R") {
            eprintln!("save_bedien: cannot write {}", save_path("BEDIEN.A2R").display());
        }
    }

    pub fn init_bedien(&mut self) {
        self.available_joy = sys_joystick_position(0).is_some();
        let mut buf = [0u8; Bedien::SIZE];
        if file_exist("BEDIEN.A2R") && file_read(&mut buf, "BEDIEN.A2R") {
            self.control = Bedien::read(&mut Reader { p: &buf });
            return;
        }
        file_delete("BEDIEN.A2R");
        self.control = Bedien {
            control_type: 0,
            field_08: 1,
            key_left: 0x25,    // VK_LEFT
            key_right: 0x27,   // VK_RIGHT
            key_gas: 0x26,     // VK_UP
            key_brake: 0x28,   // VK_DOWN
            key_action1: 0x20, // VK_SPACE
            key_action2: 0x0d, // VK_RETURN
            ..Bedien::default()
        };
        self.save_bedien();
    }
}
