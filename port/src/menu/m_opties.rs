//! The options menu (m_opties.cpp): display mode, sound, horizon, detail, dashboard and
//! volumes. menu_opties's switch is taken from the machine code (Ghidra could not
//! follow its jump table).

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::a2types::*;
use crate::menu::forms::*;
use crate::menu::menu_data::MO_ENTREE_ANIM_TABEL;
use crate::menu::menu_util::load_pcx_file_1;
use crate::menu::mysound::{snd_play, snd_play_looping, snd_set_volume, snd_stop, snd_volume_for_level, snd_wait};
use crate::menu::Menu;
use crate::platform::sys::{sys_poll, sys_sleep, sys_ticks, SysInput};
use crate::xvga::fileio::{file_delete, file_write};

// Controls of mo_form_*, by index (the same in all three forms).
const MO_OK: i32 = 0;
/// "Annuleren"
const MO_CANCEL: i32 = 1;
const MO_HELP: i32 = 2;
const MO_DISPLAY_3D: i32 = 3;
const MO_DISPLAY_320: i32 = 4;
const MO_DISPLAY_640: i32 = 5;
const MO_HORIZON_ON: i32 = 6;
const MO_HORIZON_OFF: i32 = 7;
const MO_SOUND_EFFECTS: i32 = 8;
const MO_SOUND_MUSIC: i32 = 9;
const MO_SOUND_ALL: i32 = 10;
const MO_SOUND_OFF: i32 = 11;
const MO_DETAIL_ON: i32 = 12;
const MO_DETAIL_OFF: i32 = 13;
const MO_DASHBOARD_ON: i32 = 14;
const MO_DASHBOARD_OFF: i32 = 15;
const MO_MUSIC_DOWN: i32 = 16;
const MO_MUSIC_UP: i32 = 17;
const MO_EFFECTS_DOWN: i32 = 18;
const MO_EFFECTS_UP: i32 = 19;

/// Sets a setting if it changed; returns whether it did.
fn set_option(field: &mut i16, value: i16) -> bool {
    if *field == value {
        return false;
    }
    *field = value;
    true
}

impl Menu {
    fn mo_af_image(&self, anim: i32, frame: i32) -> i32 {
        self.anim_image(anim, frame)
    }

    /// The form matching the display mode: each shows only the settings that apply.
    fn mc_active_form(&mut self) {
        self.destroy_form();
        let form = match self.opties.display_mode {
            0 => self.md.mo_form_3D.clone(),
            2 => self.md.mo_form_640.clone(),
            _ => self.md.mo_form_320.clone(),
        };
        self.activate_form(form);
        self.focus_control(MO_OK);
    }

    /// Radio lights: anim 0x3b frame 1 when the setting has that value.
    fn radio(&self, x: i32, y: i32, on: bool) {
        set_mem_image(1, self.mo_af_image(0x3b, on as i32), x, y, MI_TOPLEFT);
    }

    fn mc_display_geluid(&self) {
        set_mem_image(1, self.mo_af_image(0x43, 0), 0x10c, 0xf5, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x43, 1), 0x15d, 0xf5, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x43, 2), 0x15d, 0x10a, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x43, 3), 0x15d, 0x11f, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x43, 4), 0x15d, 0x134, MI_TOPLEFT);
        let s = self.opties.sound;
        self.radio(0x14a, 0xf6, s == SOUND_EFFECTS_ONLY);
        self.radio(0x14a, 0x10b, s == SOUND_MUSIC_ONLY);
        self.radio(0x14a, 0x120, s == SOUND_ALL);
        self.radio(0x14a, 0x135, s == SOUND_OFF);
    }

    fn mc_display_horizon(&self) {
        set_mem_image(1, self.mo_af_image(0x45, 0), 0x101, 0xa4, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x45, 1), 0x15d, 0xa4, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x45, 2), 0x15d, 0xb8, MI_TOPLEFT);
        self.radio(0x14a, 0xa5, self.opties.horizon == 1);
        self.radio(0x14a, 0xb9, self.opties.horizon == 0);
    }

    fn mc_display_detail(&self) {
        set_mem_image(1, self.mo_af_image(0x44, 0), 0x1dc, 0x164, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x44, 1), 0x228, 0x164, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x44, 2), 0x228, 0x179, MI_TOPLEFT);
        self.radio(0x215, 0x165, self.opties.detail == 1);
        self.radio(0x215, 0x17a, self.opties.detail == 0);
    }

    fn mc_display_dashbord(&self) {
        set_mem_image(1, self.mo_af_image(0x46, 0), 0xe9, 0x164, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x46, 1), 0x15d, 0x164, MI_TOPLEFT);
        set_mem_image(1, self.mo_af_image(0x46, 2), 0x15d, 0x179, MI_TOPLEFT);
        self.radio(0x14a, 0x165, self.opties.dashboard == 1);
        self.radio(0x14a, 0x17a, self.opties.dashboard == 0);
    }

    /// The settings that apply to the display mode.
    fn mc_display_inhoud(&self) {
        blt_scr_2_scr(2, 1, 0xe6, 0x96, 600, 0x19a, 0);
        self.mc_display_geluid();
        match self.opties.display_mode {
            0 => {
                self.mc_display_dashbord();
                self.mc_display_horizon();
            }
            2 => {
                self.mc_display_horizon();
                self.mc_display_detail();
                self.mc_display_dashbord();
            }
            m if m < 2 => self.mc_display_detail(),
            _ => {}
        }
        blt_scr_2_scr(1, 0, 0xe6, 0x96, 600, 0x19a, 0);
        xvga_show();
    }

    /// The volume lights on the left: lit up to the volume, off for a muted part.
    fn mc_display_snd_setting(&self) {
        let o = self.opties;
        set_mem_image(1, self.mo_af_image(0x36, (o.sound != SOUND_OFF) as i32), 0, 0x78, MI_TOPLEFT);
        if o.sound != SOUND_OFF {
            for i in 0..5 {
                let x = 0x2e + i * 0x1a;
                let anim = if (o.music_volume as i32) < i || o.sound == SOUND_EFFECTS_ONLY { 0x38 } else { 0x37 };
                set_mem_image(1, self.mo_af_image(anim, i), x, 0x9f, MI_TOPLEFT);
                let anim = if (o.effects_volume as i32) < i || o.sound == SOUND_MUSIC_ONLY { 0x38 } else { 0x37 };
                set_mem_image(1, self.mo_af_image(anim, i), x, 0xe5, MI_TOPLEFT);
            }
        }
        blt_scr_2_scr(1, 0, 0, 0x78, 0xcb, 0xf8, 0);
        xvga_show();
    }

    /// The tablet with the three display modes.
    fn mc_display_tableau(&self) {
        let frame = match self.opties.display_mode {
            0 => 0,
            m if m < 2 => 1,
            _ => 2,
        };
        set_mem_image(2, self.mo_af_image(0x35, frame), 0, 0x70, MI_TOPLEFT);
        set_mem_image(0, self.mo_af_image(0x35, frame), 0, 0x70, MI_TOPLEFT);
        self.mc_display_inhoud();
        self.mc_display_snd_setting();
    }

    fn mo_animate_buttons(&self) {
        for step in 0..8 {
            let t = sys_ticks();
            set_mem_image(0, self.mo_af_image(0x3f, MO_ENTREE_ANIM_TABEL[0][step] as i32), 0, 0x14b, MI_TOPLEFT);
            set_mem_image(0, self.mo_af_image(0x40, MO_ENTREE_ANIM_TABEL[1][step] as i32), 0, 0x17e, MI_TOPLEFT);
            xvga_show();
            let mut input = SysInput::default();
            loop {
                sys_poll(&mut input);
                sys_sleep(1);
                if sys_ticks().wrapping_sub(t) >= 0x21 {
                    break;
                }
            }
        }
    }

    fn mo_open_screen(&mut self, g: &Game) {
        load_pcx_file_1(g, 2, "Opties.pcx", 0x1f);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.mc_display_tableau();
        self.mc_display_snd_setting();
        self.mo_animate_buttons();
        self.mc_active_form();
        self.focus_control(MO_OK);
    }

    fn mo_close_screen(&mut self) {
        wait_form_key_released();
        self.destroy_form();
    }

    /// The options menu; the menu music (tune) is restarted or stopped as the sound
    /// option changes.
    pub fn menu_opties(&mut self, g: &Game, tune: &str) -> i32 {
        let mut snd = -1;
        let mut cancel = false;
        // save_tmp_optie: Annuleren puts these back.
        let saved = self.opties;
        let mut tmp = Vec::new();
        self.opties.write(&mut tmp);
        file_write(&tmp, "Opties.TMP");
        self.mo_open_screen(g);
        let mut input = SysInput::default();
        let mut running = true;
        while running {
            sys_poll(&mut input);
            let r = self.handle_form(&input);
            match r {
                FORM_UP | FORM_DOWN => {
                    // The arrow keys move between OK and Annuleren.
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    let f = if self.get_control_focus() == MO_OK { MO_CANCEL } else { MO_OK };
                    self.focus_control(f);
                    snd_wait(self.snd(), &mut snd);
                }
                FORM_ESCAPE | FORM_QUIT | MO_CANCEL | MO_OK => {
                    if r == FORM_ESCAPE || r == FORM_QUIT {
                        self.focus_control(MO_CANCEL);
                    }
                    if r != MO_OK {
                        cancel = true;
                    }
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    running = false;
                    snd_wait(self.snd(), &mut snd);
                }
                MO_HELP => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    wait_form_key_released();
                    snd_set_volume(self.snd(), &mut self.mm_tune_buffer, -8000);
                    let help = self.md.mo_help.clone();
                    self.play_help(&help, 1);
                    snd_set_volume(self.snd(), &mut self.mm_tune_buffer, snd_volume_for_level(self.opties.music_volume));
                    snd_wait(self.snd(), &mut snd);
                }
                MO_DISPLAY_3D | MO_DISPLAY_640 => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    if set_option(&mut self.opties.display_mode, if r == MO_DISPLAY_3D { 0 } else { 2 }) {
                        self.mc_display_tableau();
                        self.mc_active_form();
                    }
                }
                MO_DISPLAY_320 => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    if self.opties.display_mode != 1 {
                        self.opties.horizon = 0;
                        self.opties.dashboard = 0;
                        self.opties.display_mode = 1;
                        self.mc_display_tableau();
                        self.mc_active_form();
                    }
                }
                MO_HORIZON_ON | MO_HORIZON_OFF => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    if set_option(&mut self.opties.horizon, (r == MO_HORIZON_ON) as i16) {
                        self.mc_display_inhoud();
                    }
                }
                MO_SOUND_EFFECTS | MO_SOUND_MUSIC | MO_SOUND_ALL | MO_SOUND_OFF => {
                    const VALUE: [i16; 4] = [SOUND_EFFECTS_ONLY, SOUND_MUSIC_ONLY, SOUND_ALL, SOUND_OFF];
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    if set_option(&mut self.opties.sound, VALUE[(r - MO_SOUND_EFFECTS) as usize]) {
                        self.mc_display_snd_setting();
                        self.mc_display_inhoud();
                        if self.opties.sound == SOUND_EFFECTS_ONLY || self.opties.sound == SOUND_OFF {
                            snd_stop(&mut self.mm_tune_buffer);
                        } else {
                            snd_play_looping(self.snd(), &mut self.mm_tune_buffer, tune);
                        }
                    }
                }
                MO_DETAIL_ON | MO_DETAIL_OFF => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    let m = self.opties.display_mode;
                    if (m == 1 || m == 2) && set_option(&mut self.opties.detail, (r == MO_DETAIL_ON) as i16) {
                        self.mc_display_inhoud();
                    }
                }
                MO_DASHBOARD_ON | MO_DASHBOARD_OFF => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    let m = self.opties.display_mode;
                    if (m == 0 || m == 2) && set_option(&mut self.opties.dashboard, (r == MO_DASHBOARD_ON) as i16) {
                        self.mc_display_inhoud();
                    }
                }
                MO_MUSIC_DOWN | MO_MUSIC_UP => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    if self.opties.sound == SOUND_ALL || self.opties.sound == SOUND_MUSIC_ONLY {
                        if r == MO_MUSIC_DOWN && self.opties.music_volume > 0 {
                            self.opties.music_volume -= 1;
                        }
                        if r == MO_MUSIC_UP && self.opties.music_volume < 4 {
                            self.opties.music_volume += 1;
                        }
                        snd_set_volume(self.snd(), &mut self.mm_tune_buffer, snd_volume_for_level(self.opties.music_volume));
                        self.mc_display_snd_setting();
                    }
                }
                MO_EFFECTS_DOWN | MO_EFFECTS_UP => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    if self.opties.sound == SOUND_ALL || self.opties.sound == SOUND_EFFECTS_ONLY {
                        if r == MO_EFFECTS_DOWN && self.opties.effects_volume > 0 {
                            self.opties.effects_volume -= 1;
                        }
                        if r == MO_EFFECTS_UP && self.opties.effects_volume < 4 {
                            self.opties.effects_volume += 1;
                        }
                        snd_set_volume(self.snd(), &mut snd, snd_volume_for_level(self.opties.effects_volume));
                        self.mc_display_snd_setting();
                    }
                }
                _ => {}
            }
            xvga_show();
            sys_sleep(10);
        }
        self.mo_close_screen();
        if cancel {
            // load_tmp_optie, then the music follows the restored setting.
            let changed = self.opties.sound;
            self.opties = saved;
            let restored = self.opties.sound;
            let plays = |s: i16| s == SOUND_ALL || s == SOUND_MUSIC_ONLY;
            if plays(restored) && !plays(changed) {
                snd_play_looping(self.snd(), &mut self.mm_tune_buffer, tune);
            }
            if !plays(restored) && plays(changed) {
                snd_stop(&mut self.mm_tune_buffer);
            }
            snd_set_volume(self.snd(), &mut self.mm_tune_buffer, snd_volume_for_level(self.opties.music_volume));
        }
        file_delete("Opties.TMP");
        self.save_optie();
        0
    }
}
