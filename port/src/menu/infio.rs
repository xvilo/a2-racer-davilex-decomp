//! INFIO.INI (infio.cpp): what the menu and the race tell each other. The menu writes
//! the player's options, controls and standing before a race; the race writes back
//! whether it was finished, the times and the positions.

use crate::menu::menu_data::TOETS_DEFINE;
use crate::xvga::fileio::{file_delete, file_exist};
use crate::xvga::inifile::{read_ini_value_setting, set_ini_filename, write_ini_value_setting};

/// infio_t in A2MENU.DAT; the INI keys are in the comments.
#[derive(Clone, Copy, Debug, Default)]
pub struct Infio {
    /// GAME/RACE_KLAAR
    pub race_finished: u8,
    /// GAME/TIJD, seconds
    pub time: i32,
    /// GAME/TIJD_1 .. TIJD_5
    pub time_racer: [i32; 5],
    /// GAME/ACTIVE_SLOT
    pub active_slot: i16,
    /// OPTION/GELUID
    pub sound: i16,
    /// OPTION/RESOLUTIE
    pub resolution: i16,
    /// OPTION/VIEW
    pub view: i16,
    /// CONTROL/BEDIENING
    pub control_type: i16,
    /// CONTROL/KEY_LINKS, KEY_RECHTS, KEY_GAS, KEY_REM, KEY_ACTIE1, KEY_ACTIE2
    pub keys: [i16; 6],
    /// OPTION/DISPLAY_MODE
    pub display_mode: i16,
    /// OPTION/HORIZON
    pub horizon: i16,
    /// OPTION/DASHBORD
    pub dashboard: i16,
    /// OPTION/DETAIL
    pub detail: i16,
    /// GAME/ETAPPE
    pub stage: i16,
    /// OPTION/CAMERA
    pub camera: i16,
    /// GAME/EINDE_DOOR: 0 quit, 1 fuel, 2 money, 3 time
    pub ended_by: i16,
    /// GAME/POSITIE
    pub position: i16,
    /// GAME/RACER_1 .. RACER_5: the rivals' positions
    pub racer: [i16; 5],
    /// GAME/FINALE
    pub final_: i16,
    /// OPTION/VMUZIEK
    pub music_volume: i16,
    /// OPTION/VEFFECT
    pub effects_volume: i16,
    /// GAME/GELD
    pub money: i32,
}

const KEY_NAMES: [&str; 6] = ["KEY_LINKS", "KEY_RECHTS", "KEY_GAS", "KEY_REM", "KEY_ACTIE1", "KEY_ACTIE2"];

impl Infio {
    pub fn key_left(&self) -> i16 { self.keys[0] }
    pub fn key_right(&self) -> i16 { self.keys[1] }
    pub fn key_gas(&self) -> i16 { self.keys[2] }
    pub fn key_brake(&self) -> i16 { self.keys[3] }

    pub fn load_infio(&mut self) {
        if !file_exist("INFIO.INI") {
            file_delete("INFIO.INI");
            self.init_infio();
            return;
        }
        set_ini_filename("INFIO.INI");
        let g = |k: &str| read_ini_value_setting("GAME", k);
        self.race_finished = g("RACE_KLAAR") as u8;
        self.time = g("TIJD") as i32;
        self.active_slot = g("ACTIVE_SLOT") as i16;
        self.money = g("GELD") as i32;
        self.stage = g("ETAPPE") as i16;
        self.ended_by = g("EINDE_DOOR") as i16;
        self.position = g("POSITIE") as i16;
        for i in 0..5 {
            self.racer[i] = g(&format!("RACER_{}", i + 1)) as i16;
            // The rivals' times go through a short, as in the original.
            self.time_racer[i] = g(&format!("TIJD_{}", i + 1)) as i16 as i32;
        }
        self.final_ = g("FINALE") as i16;
        self.control_type = read_ini_value_setting("CONTROL", "BEDIENING") as i16;
        for (i, k) in KEY_NAMES.iter().enumerate() {
            self.keys[i] = read_ini_value_setting("CONTROL", k) as i16;
        }
        let o = |k: &str| read_ini_value_setting("OPTION", k) as i16;
        self.sound = o("GELUID");
        self.resolution = o("RESOLUTIE");
        self.view = o("VIEW");
        self.display_mode = o("DISPLAY_MODE");
        self.horizon = o("HORIZON");
        self.dashboard = o("DASHBORD");
        self.detail = o("DETAIL");
        self.camera = o("CAMERA");
        self.music_volume = o("VMUZIEK");
        self.effects_volume = o("VEFFECT");
    }

    pub fn save_infio(&self) {
        set_ini_filename("INFIO.INI");
        let g = |k: &str, v: i32| write_ini_value_setting("GAME", k, v);
        g("RACE_KLAAR", self.race_finished as i32);
        g("TIJD", self.time);
        g("ACTIVE_SLOT", self.active_slot as i32);
        g("GELD", self.money);
        g("ETAPPE", self.stage as i32);
        g("EINDE_DOOR", self.ended_by as i32);
        g("POSITIE", self.position as i32);
        for i in 0..5 {
            g(&format!("RACER_{}", i + 1), self.racer[i] as i32);
        }
        for i in 0..5 {
            g(&format!("TIJD_{}", i + 1), self.time_racer[i]);
        }
        g("FINALE", self.final_ as i32);
        write_ini_value_setting("CONTROL", "BEDIENING", self.control_type as i32);
        for (i, k) in KEY_NAMES.iter().enumerate() {
            write_ini_value_setting("CONTROL", k, self.keys[i] as i32);
        }
        let o = |k: &str, v: i16| write_ini_value_setting("OPTION", k, v as i32);
        o("GELUID", self.sound);
        o("RESOLUTIE", self.resolution);
        o("VIEW", self.view);
        o("DISPLAY_MODE", self.display_mode);
        o("HORIZON", self.horizon);
        o("DASHBORD", self.dashboard);
        o("DETAIL", self.detail);
        o("CAMERA", self.camera);
        o("VMUZIEK", self.music_volume);
        o("VEFFECT", self.effects_volume);
    }

    pub fn init_infio(&mut self) {
        if file_exist("INFIO.INI") {
            self.load_infio();
            return;
        }
        *self = Infio {
            racer: [1, 2, 3, 4, 5],
            music_volume: 3,
            effects_volume: 4,
            display_mode: 1,
            detail: 1,
            position: 6,
            // The original left money at whatever EDX held; reset_infio sets it before a race.
            money: 0,
            keys: TOETS_DEFINE,
            ..Infio::default()
        };
        self.save_infio();
    }
}
