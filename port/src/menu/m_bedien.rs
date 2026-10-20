//! The controls menu (m_bedien.cpp): keyboard, joypad or steering wheel, and the keys
//! for left, right, gas and brake.

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::forms::*;
use crate::menu::menu_data::{MC_ENTREE_ANIM_TABEL, MC_INSTEL_WAV};
use crate::menu::menu_util::load_pcx_file_1;
use crate::menu::mysound::{snd_play, snd_set_volume, snd_stop, snd_volume_for_level, snd_wait};
use crate::menu::txttool::win_printwrapped;
use crate::menu::Menu;
use crate::platform::sys::{keyispressed, sys_poll, sys_sleep, sys_ticks, SysInput, SCREEN_H, SCREEN_W};
use crate::xvga::fileio::{file_delete, file_write};
use crate::xvga::font::{win_print_xy, xvga_select_font, xvga_set_font_colors};

// Controls of mc_form / mc_form_joy, by index.
const MC_OK: i32 = 0;
const MC_CANCEL: i32 = 1;
const MC_HELP: i32 = 2;
const MC_KEYBOARD: i32 = 3;
const MC_JOYPAD: i32 = 4;
const MC_WHEEL: i32 = 5;
/// "Instellen", keyboard only.
const MC_CONFIGURE: i32 = 6;

/// Left, right, gas, brake.
const KEY_COUNT: usize = 4;

/// Waits 33 ms per step, keeping the window responsive.
fn wait_step(t: u32) {
    let mut input = SysInput::default();
    loop {
        sys_poll(&mut input);
        sys_sleep(1);
        if sys_ticks().wrapping_sub(t) >= 0x21 {
            break;
        }
    }
}

impl Menu {
    fn mc_af_image(&self, anim: i32, frame: i32) -> i32 {
        self.anim_image(anim, frame)
    }

    /// The text bar on the left (anim 0x27 frame 7: fully open).
    fn mc_clear_text_bar(&self) {
        set_mem_image(0, self.mc_af_image(0x27, 7), 0x17, 0x79, MI_TOPLEFT);
        xvga_show();
    }

    fn mc_print_text_bar(&self, text: &[u8]) {
        set_mem_image(1, self.mc_af_image(0x27, 7), 0x17, 0x79, MI_TOPLEFT);
        xvga_select_font(3);
        xvga_set_font_colors(3, 0, 0x5a32ff);
        win_printwrapped(1, 0x2b, 0x8d, 200, text);
        blt_scr_2_scr(1, 0, 0x17, 0x79, 0x10e, 0xe2, 0);
        xvga_show();
    }

    /// Index of a key in key_list, 0 (no key) when it is not there.
    fn scan_2_ckey(&self, vk: i32) -> usize {
        self.md.key_list.iter().position(|k| k.vk == vk).unwrap_or(0)
    }

    fn key_name(&self, vk: i32) -> &'static [u8] {
        self.md.key_list[self.scan_2_ckey(vk)].name.unwrap_or(b"")
    }

    fn load_ckey(&mut self) {
        for i in 0..KEY_COUNT {
            self.md.config_key[i].key = self.control.keys()[i];
        }
    }

    fn save_ckey(&mut self) {
        for i in 0..KEY_COUNT {
            *self.control.key_mut(i) = self.md.config_key[i].key;
        }
    }

    fn print_keys(&self) {
        xvga_select_font(3);
        for k in &self.md.config_key[..KEY_COUNT] {
            set_mem_image(0, self.mc_af_image(0x34, 0), k.x - 8, k.y - 8, MI_TOPLEFT);
            xvga_set_font_colors(3, 0, 0xfaf0ff);
            let name = self.key_name(k.key);
            win_print_xy(0, k.x, k.y, &format!("{:>8}", String::from_utf8_lossy(name)).into_bytes());
        }
        xvga_show();
    }

    fn clear_keys(&self) {
        self.mc_clear_text_bar();
        for k in &self.md.config_key[..KEY_COUNT] {
            set_mem_image(0, self.mc_af_image(0x34, 0), k.x - 8, k.y - 8, MI_TOPLEFT);
        }
        xvga_show();
    }

    fn clear_all_ckey(&mut self) {
        for k in &mut self.md.config_key[..KEY_COUNT] {
            k.key = -1;
        }
    }

    /// Which setting already uses this key, or None.
    fn ckey_new_define(&self, vk: i32) -> Option<usize> {
        self.md.config_key[..KEY_COUNT].iter().position(|k| k.key == vk)
    }

    /// Waits for Escape or one of the keys in key_list, and its release.
    fn mb_read_key(&self) -> i32 {
        let mut input = SysInput::default();
        loop {
            sys_poll(&mut input);
            xvga_show();
            if input.quit || keyispressed(0x1b) {
                while keyispressed(0x1b) {
                    sys_sleep(5);
                }
                return 0x1b;
            }
            for k in &self.md.key_list[1..] {
                if keyispressed(k.vk) {
                    while keyispressed(k.vk) {
                        sys_poll(&mut input);
                        sys_sleep(5);
                    }
                    return k.vk;
                }
            }
            sys_sleep(5);
        }
    }

    fn animate_textbox(&self, from: i32, to: i32, step: i32) {
        let mut frame = from;
        while frame != to + step {
            let t = sys_ticks();
            set_mem_image(0, self.mc_af_image(0x27, frame), 0x17, 0x79, MI_TOPLEFT);
            xvga_show();
            wait_step(t);
            frame += step;
        }
    }

    fn mc_music(&mut self, level: i16) {
        snd_set_volume(self.snd(), &mut self.mm_tune_buffer, snd_volume_for_level(level));
    }

    /// "Instellen": asks for the four keys one by one.
    fn config_keys(&mut self) -> bool {
        let (mut voice, mut click) = (-1, -1);
        self.animate_textbox(0, 7, 1);
        self.mc_music(self.opties.music_volume / 2);
        self.clear_all_ckey();
        let mut i = 0;
        while i < KEY_COUNT {
            self.print_keys();
            snd_play(self.snd(), &mut voice, &String::from_utf8_lossy(MC_INSTEL_WAV[i]));
            let mut text = b"Kies nu een toets voor:\n ".to_vec();
            text.extend_from_slice(self.md.config_key[i].label.unwrap_or(b""));
            self.mc_print_text_bar(&text);
            let vk = self.mb_read_key();
            if vk == 0x1b {
                snd_stop(&mut click);
                snd_play(self.snd(), &mut click, "click1.wav");
                self.load_ckey();
                self.print_keys();
                self.animate_textbox(7, 0, -1);
                snd_wait(self.snd(), &mut click);
                self.mc_music(self.opties.music_volume);
                return false;
            }
            snd_play(self.snd(), &mut click, "click3.wav");
            if self.ckey_new_define(vk).is_some() {
                self.clear_keys();
                snd_play(self.snd(), &mut voice, "MC_09.wav");
                let mut w = b"Deze toets, <".to_vec();
                w.extend_from_slice(self.key_name(vk));
                w.extend_from_slice(b">, is eerder gedefinieerd. Kies een andere toets.");
                self.req_warning(&w);
                snd_stop(&mut voice);
                continue;
            }
            self.md.config_key[i].key = vk;
            i += 1;
            self.print_keys();
        }
        snd_play(self.snd(), &mut voice, "MC_10.wav");
        self.mc_print_text_bar(b"De toetsen zijn ingesteld.");
        snd_wait(self.snd(), &mut voice);
        self.animate_textbox(7, 0, -1);
        self.mc_music(self.opties.music_volume);
        true
    }

    /// The picture of the chosen device (anim 0x33: keyboard, joypad, wheel, keyboard only).
    fn display_device(&self, frame: i32) {
        set_mem_image(2, self.mc_af_image(0x33, frame), 0x132, 0x7d, MI_TOPLEFT);
        set_mem_image(0, self.mc_af_image(0x33, frame), 0x132, 0x7d, MI_TOPLEFT);
        xvga_show();
    }

    fn display_keys(&self) {
        self.display_device(if self.available_joy { 0 } else { 3 });
        self.print_keys();
    }

    fn mc_display_controls(&mut self) {
        if !self.available_joy {
            self.control.control_type = 0;
        }
        if self.control.control_type == 0 {
            self.display_keys();
            let st = if self.available_joy { 3 } else { 0 };
            self.md.mc_form[4].state = st;
            self.md.mc_form[5].state = st;
        } else {
            self.display_device(if self.control.control_type == 1 { 1 } else { 2 });
            for i in 3..=5 {
                self.md.mc_form_joy[i].state = 3;
            }
        }
    }

    fn mc_animate_buttons(&self) {
        for step in 0..8 {
            let t = sys_ticks();
            set_mem_image(0, self.mc_af_image(0x2a, MC_ENTREE_ANIM_TABEL[0][step] as i32), 0, 0x14b, MI_TOPLEFT);
            set_mem_image(0, self.mc_af_image(0x2b, MC_ENTREE_ANIM_TABEL[1][step] as i32), 0, 0x17e, MI_TOPLEFT);
            xvga_show();
            wait_step(t);
        }
    }

    fn mc_open_screen(&mut self, g: &Game) {
        load_pcx_file_1(g, 2, "bedien.pcx", 0x1f);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        blt_scr_2_scr(2, 1, 0, 0, SCREEN_W as i32, SCREEN_H as i32, 0); // do_back_screen_update
        self.load_ckey();
        self.mc_display_controls();
        self.mc_animate_buttons();
        let form = if self.control.control_type == 0 { self.md.mc_form.clone() } else { self.md.mc_form_joy.clone() };
        self.activate_form(form);
        self.focus_control(0);
    }

    fn switch_device(&mut self, ty: i32) {
        self.control.control_type = ty;
        self.destroy_form();
        if ty == 0 {
            self.display_keys();
            self.activate_form(self.md.mc_form.clone());
        } else {
            self.display_device(ty);
            self.activate_form(self.md.mc_form_joy.clone());
        }
        self.focus_control(0);
    }

    /// The controls menu; the menu music is turned down while help and voices play.
    pub fn menu_bedien(&mut self, g: &Game) -> i32 {
        let mut snd = -1;
        let mut cancel = false;
        let saved = self.control; // save_tmp_bedien: BEDIEN.TMP
        let mut tmp = Vec::new();
        self.control.write(&mut tmp);
        file_write(&tmp, "BEDIEN.TMP");
        self.mc_open_screen(g);
        let mut input = SysInput::default();
        let mut running = true;
        while running {
            sys_poll(&mut input);
            let r = self.handle_form(&input);
            match r {
                MC_OK => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    running = false;
                    snd_wait(self.snd(), &mut snd);
                }
                FORM_ESCAPE | FORM_QUIT | MC_CANCEL => {
                    if r != MC_CANCEL {
                        self.focus_control(MC_CANCEL);
                    }
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    running = false;
                    cancel = true;
                    snd_wait(self.snd(), &mut snd);
                }
                MC_HELP => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut snd);
                    snd_set_volume(self.snd(), &mut self.mm_tune_buffer, -8000);
                    let help = if self.available_joy { self.md.mc_help.clone() } else { self.md.mc_help2.clone() };
                    self.play_help(&help, 1);
                    self.mc_music(self.opties.music_volume);
                }
                MC_KEYBOARD => {
                    snd_play(self.snd(), &mut snd, "click2.wav");
                    wait_form_key_released();
                    if self.control.control_type != 0 {
                        self.switch_device(0);
                    }
                    snd_wait(self.snd(), &mut snd);
                }
                MC_JOYPAD | MC_WHEEL => {
                    if self.available_joy {
                        snd_play(self.snd(), &mut snd, "click2.wav");
                        wait_form_key_released();
                        if self.control.control_type != r - MC_KEYBOARD {
                            self.switch_device(r - MC_KEYBOARD);
                        }
                        snd_wait(self.snd(), &mut snd);
                    }
                }
                MC_CONFIGURE => {
                    if self.control.control_type == 0 {
                        snd_play(self.snd(), &mut snd, "click1.wav");
                        wait_form_key_released();
                        self.destroy_form();
                        self.config_keys();
                        self.display_keys();
                        self.mc_display_controls();
                        self.activate_form(self.md.mc_form.clone());
                        self.focus_control(MC_CONFIGURE);
                    }
                }
                FORM_UP | FORM_DOWN => {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    let f = if self.get_control_focus() == MC_OK { MC_CANCEL } else { MC_OK };
                    self.focus_control(f);
                    snd_wait(self.snd(), &mut snd);
                }
                _ => {}
            }
            xvga_show();
            sys_sleep(10);
        }
        wait_form_key_released();
        self.destroy_form();
        self.save_ckey();
        if cancel {
            self.control = saved; // load_tmp_bedien
        }
        file_delete("BEDIEN.TMP");
        self.save_bedien();
        0
    }
}
