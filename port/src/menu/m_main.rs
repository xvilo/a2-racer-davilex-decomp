//! The main menu (m_main.cpp).

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, clear_screen, get_mem_image_size, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::a2types::{SOUND_ALL, SOUND_MUSIC_ONLY};
use crate::menu::forms::*;
use crate::menu::menu_data::{MM_ENTREE_ANIM_TABEL, MM_TUNE};
use crate::menu::menu_types::cstr;
use crate::menu::menu_util::{load_pcx_file_1, num_left, pad_right};
use crate::menu::mysound::{snd_play, snd_play_looping, snd_set_volume, snd_stop, snd_volume_for_level, snd_wait};
use crate::menu::slot::slot_check_index;
use crate::menu::txttool::win_bprintf;
use crate::menu::Menu;
use crate::platform::sys::{sys_poll, sys_sleep, sys_ticks, SysInput, SCREEN_H, SCREEN_W};
use crate::xvga::fileio::file_delete;
use crate::xvga::font::{win_print_xy, xvga_select_font, xvga_set_font_colors};

// Controls of mm_form, by index.
const MM_HELP: i32 = 0;
/// "Einde"
const MM_EXIT: i32 = 1;
const MM_START: i32 = 2;
/// "Bediening"
const MM_CONTROLS: i32 = 3;
/// "Opties"
const MM_OPTIONS: i32 = 4;
/// "Score"
const MM_HISCORE: i32 = 5;
/// "Opnieuw": cancel the current race series.
const MM_RESTART: i32 = 6;
const MM_NEW_PLAYER: i32 = 7;

/// menu_main's results: the launcher then runs the race.
pub const MENU_START_RACE: i32 = 0x28;
pub const MENU_EXIT: i32 = 0xff;

/// Waits for the input that started an action to finish (wait_*_pressed).
fn wait_keys_released() {
    let mut input = SysInput::default();
    sys_poll(&mut input);
}

impl Menu {
    fn mm_af_image(&self, anim: i32, frame: i32) -> i32 {
        if frame < 0 || frame >= self.md.anims[anim as usize].frame_count {
            eprintln!("mm_af_image: anim {anim} has no frame {frame}");
            return self.anim_image(anim, 0);
        }
        self.anim_image(anim, frame)
    }

    /// The player's name, money, total time, controls and display in the box on the
    /// right, drawn at an offset (three passes give the shadow).
    fn mm_draw_info_text(&self, dx: i32, dy: i32) {
        let slot = slot_check_index(self.active_slot) as i32;
        let x = dx + 0x205;
        win_bprintf(1, x, dy + 0x8c, 0x5a, &pad_right(&self.slot_naam_string(slot), 16));
        win_print_xy(1, x, dy + 0x9d, &num_left(self.slot_list[slot as usize].money, 9));
        win_print_xy(1, x, dy + 0xae, &pad_right(&self.slot_tijd_string(slot), 6));
        let controls: &[u8] = match self.control.control_type {
            0 => b"Toetsenbord", // keyboard
            1 => b"Joypad 1",
            2 => b"Stuur", // steering wheel
            _ => b"Onbekend", // unknown
        };
        win_bprintf(1, dx + 0x205, dy + 0xbf, 0x5a, controls);
        let display: &[u8] = match self.opties.display_mode {
            0 => b"3D Accelerator",
            1 => b"320 x 200",
            2 => b"640 x 480",
            _ => b"Onbekend",
        };
        win_bprintf(1, dx + 0x205, dy + 0xd1, 0x5a, display);
    }

    fn mm_display_info(&self) {
        xvga_select_font(4);
        xvga_set_font_colors(4, 0, 0x28f0ff);
        blt_scr_2_scr(2, 1, 500, 0x87, 0x262, 0xeb, 0);
        xvga_set_font_colors(4, 0, 0);
        self.mm_draw_info_text(0, 0);
        self.mm_draw_info_text(-1, -1);
        xvga_set_font_colors(4, 0, 0xf5f5f5);
        self.mm_draw_info_text(-2, -2);
        blt_scr_2_scr(1, 0, 500, 0x87, 0x262, 0xeb, 0);
        xvga_show();
    }

    /// The six buttons on the left slide in, one step every 33 ms.
    fn mm_animate_buttons(&self) {
        const ANIM: [i32; 6] = [0x1a, 0x1b, 0x1c, 0x1e, 0x1d, 0x1f];
        const Y: [i32; 6] = [0x7c, 0xb0, 0xe4, 0x116, 0x14b, 0x17e];
        for step in 0..12 {
            let t = sys_ticks();
            for b in 0..6 {
                set_mem_image(0, self.mm_af_image(ANIM[b], MM_ENTREE_ANIM_TABEL[b][step] as i32), 0, Y[b], MI_TOPLEFT);
            }
            xvga_show();
            let mut input = SysInput::default();
            loop {
                sys_poll(&mut input); // handle_win_msg
                sys_sleep(1);
                if sys_ticks().wrapping_sub(t) >= 0x21 {
                    break;
                }
            }
        }
    }

    /// The map of the Netherlands with the current stage.
    fn mm_animate_map(&self, stage: i32) {
        let image = self.mm_af_image(0x26, stage);
        let Some((w, h)) = get_mem_image_size(image) else { return };
        set_mem_image(1, image, 0x15d, 0xf5, MI_TOPLEFT);
        blt_scr_2_scr(1, 0, 0x15d, 0xf5, w + 0x15d, h + 0xf5, 0);
        xvga_show();
    }

    /// first: coming from the start (with the palette) rather than back from a submenu.
    fn mm_open_screen_1(&mut self, g: &Game, first: bool) {
        load_pcx_file_1(g, 2, "hoofd.pcx", if first { 0x1f } else { 0x19 });
        handle_wipe(2, WIPE_PIXELFADE, 600);
        blt_scr_2_scr(2, 0, 0, 0, SCREEN_W as i32, SCREEN_H as i32, 0); // do_screen_update
        self.mm_display_info();
        self.mm_animate_buttons();
        self.mm_animate_map(self.slot_list[slot_check_index(self.active_slot)].stage as i32);
        self.activate_form(self.md.mm_form.clone());
        xvga_show();
    }

    /// The player's series so far, as a high-score entry.
    fn add_slot_hiscore(&mut self) -> i32 {
        let s = self.slot_list[slot_check_index(self.active_slot)];
        let time = self.slot_count_total_tijd(self.active_slot, s.stage as i32);
        self.add_new_hiscore(cstr(&s.name), time, s.stage as i32 - 1, s.money)
    }

    /// "Score": the lists, with the player's series so far added for this viewing only.
    fn mm_handle_tmp_hiscore(&mut self, g: &Game) {
        self.save_tmp_scoredbase("SCORE.TMP");
        let stage = self.slot_list[slot_check_index(self.active_slot)].stage as i32;
        if stage - 1 >= 0 {
            self.add_slot_hiscore();
        }
        self.menu_score(g, stage - 1);
        self.load_tmp_scoredbase("SCORE.TMP");
        file_delete("SCORE.TMP");
    }

    /// "Opnieuw": the series starts over. After a stage or more, the result goes into
    /// the high scores first, and the list is shown when it made the top ten.
    fn mm_handle_opnieuw_spel(&mut self, g: &Game) {
        let stage = self.slot_list[slot_check_index(self.active_slot)].stage as i32;
        if stage - 1 < 0 {
            self.reset_slot(self.active_slot);
            self.mm_display_info();
        } else {
            self.mm_close_screen(false);
            self.add_slot_hiscore();
            let rank = self.get_last_entered_hiscore();
            if (0..10).contains(&rank) {
                self.menu_score(g, stage - 1);
            }
            self.reset_slot(self.active_slot);
            self.mm_open_screen_1(g, false);
        }
    }

    /// Draws the main menu screen once without waiting for input (screenshots).
    pub fn mm_open_screen(&mut self, g: &Game) {
        self.mm_open_screen_1(g, true);
    }

    fn mm_close_screen(&mut self, clear: bool) {
        wait_keys_released();
        self.destroy_form();
        if clear {
            clear_screen(2, 0);
            handle_wipe(2, WIPE_PIXELFADE, 600);
        }
    }

    fn music_volume(&mut self, level: i16) {
        snd_set_volume(self.snd(), &mut self.mm_tune_buffer, snd_volume_for_level(level));
    }

    /// Runs the main menu until the player starts a race or leaves.
    pub fn menu_main(&mut self, g: &mut Game) -> i32 {
        let mut result = MENU_EXIT;
        let mut click = -1;
        let mut running = true;
        if self.opties.sound == SOUND_ALL || self.opties.sound == SOUND_MUSIC_ONLY {
            let stage = self.slot_list[slot_check_index(self.active_slot)].stage as usize;
            let tune = String::from_utf8_lossy(MM_TUNE[stage]).into_owned();
            snd_play_looping(self.snd(), &mut self.mm_tune_buffer, &tune);
        }
        self.mm_open_screen_1(g, true);
        let mut input = SysInput::default();
        while running {
            sys_poll(&mut input);
            let r = self.handle_form(&input);
            match r {
                FORM_QUIT => running = false,
                MM_HELP => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut click);
                    snd_set_volume(self.snd(), &mut self.mm_tune_buffer, -8000);
                    let help = self.md.mm_help.clone();
                    self.play_help(&help, 1);
                    self.music_volume(self.opties.music_volume);
                }
                MM_EXIT => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    running = false;
                }
                MM_START => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut click);
                    self.reset_infio(g);
                    result = MENU_START_RACE;
                    running = false;
                }
                MM_RESTART => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut click);
                    self.music_volume(self.opties.music_volume / 2);
                    snd_play(self.snd(), &mut click, "MM_07.wav");
                    if self.req_vraag(b"De huidige race wordt afgelast. Alle coureurs worden geacht opnieuw te beginnen.\nWeet je zeker dat je opnieuw wil beginnen?") {
                        snd_stop(&mut click);
                        self.mm_handle_opnieuw_spel(g);
                    }
                    self.music_volume(self.opties.music_volume);
                    snd_stop(&mut click);
                    self.activate_form(self.md.mm_form.clone());
                }
                MM_NEW_PLAYER => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut click);
                    self.music_volume(self.opties.music_volume / 2);
                    snd_play(self.snd(), &mut click, "MM_06.wav");
                    if self.req_vraag(b"Weet je zeker dat je de huidige race wil onderbreken?") {
                        snd_stop(&mut click);
                        self.music_volume(self.opties.music_volume);
                        self.mm_close_screen(false);
                        self.menu_nieuwesplr(g);
                        self.mm_open_screen_1(g, false);
                    } else {
                        snd_stop(&mut click);
                        self.music_volume(self.opties.music_volume);
                        self.activate_form(self.md.mm_form.clone());
                    }
                }
                MM_OPTIONS => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    self.mm_close_screen(false);
                    snd_wait(self.snd(), &mut click);
                    let stage = self.slot_list[slot_check_index(self.active_slot)].stage as usize;
                    let tune = String::from_utf8_lossy(MM_TUNE[stage]).into_owned();
                    self.menu_opties(g, &tune);
                    self.update_active_slot_optie();
                    self.mm_open_screen_1(g, false);
                }
                MM_CONTROLS => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    self.mm_close_screen(false);
                    snd_wait(self.snd(), &mut click);
                    self.menu_bedien(g);
                    self.update_active_slot_bediening();
                    self.mm_open_screen_1(g, false);
                }
                MM_HISCORE => {
                    snd_play(self.snd(), &mut click, "click1.wav");
                    self.mm_close_screen(false);
                    snd_wait(self.snd(), &mut click);
                    self.mm_handle_tmp_hiscore(g);
                    self.mm_open_screen_1(g, false);
                }
                FORM_ESCAPE => {
                    // Escape moves to "Einde" and leaves.
                    self.focus_control(MM_EXIT);
                    snd_play(self.snd(), &mut click, "click1.wav");
                    running = false;
                }
                FORM_UP | FORM_DOWN => {
                    // Arrow keys cycle through the buttons on the left, Start to New player.
                    snd_play(self.snd(), &mut click, "click3.wav");
                    let mut f = self.get_control_focus();
                    if r == FORM_DOWN {
                        f = if (MM_START..MM_NEW_PLAYER).contains(&f) { f + 1 } else { MM_START };
                    } else {
                        f = if f > MM_START && f <= MM_NEW_PLAYER { f - 1 } else { MM_NEW_PLAYER };
                    }
                    self.focus_control(f);
                }
                _ => {}
            }
            xvga_show();
            sys_sleep(10);
        }
        snd_wait(self.snd(), &mut click);
        self.mm_close_screen(true);
        snd_stop(&mut self.mm_tune_buffer);
        result
    }
}
