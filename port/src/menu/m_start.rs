//! The start screen (m_start.cpp): pick, create or delete a player.

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, clear_screen, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::a2types::SLOT_COUNT;
use crate::menu::forms::*;
use crate::menu::menu_util::{load_pcx_file_1, num_right, pad_right};
use crate::menu::mysound::{snd_play, snd_stop, snd_wait};
use crate::menu::slot::slot_check_index;
use crate::menu::txttool::win_bprintf;
use crate::menu::Menu;
use crate::platform::sys::{keyispressed, sys_poll, sys_sleep, SysInput};
use crate::xvga::font::{xvga_select_font, xvga_set_font_colors};
use crate::xvga::sound::sound_buffer_busy;

// Controls of mst_form, by index; 4..13 are the ten slot rows.
const MST_OK: i32 = 0;
/// "Verwijder"
const MST_DELETE: i32 = 1;
const MST_HELP: i32 = 2;
/// "Einde"
const MST_EXIT: i32 = 3;
const MST_SLOT0: i32 = 4;

pub const ASK_NAME: &[u8] = b"Vul je naam in en wordt lid van de A2 Racing Club!";

impl Menu {
    /// One slot row: the name, the stage, the money and the total time.
    fn mst_draw_slot_text(&self, slot: i32) {
        let p = self.md.mst_slot_list[slot as usize];
        win_bprintf(1, p.x + 10, p.y, 0x80, &pad_right(&self.slot_naam_string(slot), 16));
        win_bprintf(1, p.x + 0x94, p.y, 0xfa, &pad_right(&self.slot_level_string(slot), 16));
        win_bprintf(1, p.x + 0xfe, p.y, 0xfa, &num_right(self.slot_list[slot as usize].money, 9));
        win_bprintf(1, p.x + 0x189, p.y, 0x80, &pad_right(&self.slot_tijd_string(slot), 6));
    }

    fn mst_draw_slot_info(&self, slot: i32) {
        let p = self.md.mst_slot_list[slot as usize];
        set_mem_image(1, self.anim_image(5, 0), p.x, p.y, MI_TOPLEFT);
        xvga_select_font(2);
        xvga_set_font_colors(2, 0xafaff, 0xafaff);
        self.mst_draw_slot_text(slot);
    }

    /// The selected row has a highlighted bar and white text.
    fn mst_draw_selected_slot_info(&self, slot: i32) {
        let p = self.md.mst_slot_list[slot as usize];
        set_mem_image(1, self.anim_image(5, 1), p.x, p.y, MI_TOPLEFT);
        xvga_select_font(2);
        xvga_set_font_colors(2, 0xfafafa, 0xfafafa);
        self.mst_draw_slot_text(slot);
    }

    fn mst_reselect_slot(&self, old_slot: i32, new_slot: i32) {
        blt_scr_2_scr(0, 1, 0x42, 0x8d, 0x23b, 0x17d, 0);
        self.mst_draw_slot_info(old_slot);
        self.mst_draw_selected_slot_info(new_slot);
        blt_scr_2_scr(1, 0, 0x42, 0x8d, 0x23b, 0x17d, 0);
        xvga_show();
    }

    fn mst_print_slot(&self, selected: i32) {
        blt_scr_2_scr(2, 1, 0x42, 0x8d, 0x23b, 0x17d, 0);
        xvga_select_font(2);
        for i in 0..SLOT_COUNT as i32 {
            self.mst_draw_slot_info(i);
        }
        self.mst_draw_selected_slot_info(selected);
        blt_scr_2_scr(1, 0, 0x42, 0x8d, 0x23b, 0x17d, 0);
        xvga_show();
    }

    /// The A2 logo with its jingle, the first time the game is started.
    fn mst_handle_intro(&self, g: &Game, show: bool) {
        if !show {
            return;
        }
        load_pcx_file_1(g, 2, "LOGO256.PCX", 0x1f);
        clear_screen(0, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        let mut snd = -1;
        snd_play(self.snd(), &mut snd, "algintro.wav");
        let mut input = SysInput::default();
        while sound_buffer_busy(snd) && !keyispressed(0x20) && !keyispressed(0x1b) {
            sys_poll(&mut input);
            if input.quit {
                break;
            }
            xvga_show();
            sys_sleep(10);
        }
        snd_stop(&mut snd);
        clear_screen(2, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
    }

    fn mst_open_screen(&mut self, g: &Game, selected: i32) {
        load_pcx_file_1(g, 2, "NSPEL_1.PCX", 0x1f);
        clear_screen(0, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.mst_print_slot(selected);
        self.activate_form(self.md.mst_form.clone());
    }

    fn mst_close_screen(&mut self) {
        wait_form_key_released();
        self.destroy_form();
        clear_screen(2, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
    }

    /// first: started with the FIRST command-line flag (first run after installing).
    /// False when the player chose "Einde".
    pub fn menu_start(&mut self, g: &Game, first: bool) -> bool {
        let mut snd = -1;
        let mut quit = false;
        let mut slot = 0;
        if first {
            slot = if self.slot_list[slot_check_index(self.get_last_saved_slot())].used != 0 { 0 } else { 1 };
        }
        self.mst_handle_intro(g, slot != 0);
        slot = self.get_last_saved_slot();
        self.mst_open_screen(g, slot);
        if first {
            snd_play(self.snd(), &mut snd, "MNS_00.wav");
        }

        // No players yet: ask for a name straight away.
        if self.slot_list[slot_check_index(self.get_last_saved_slot())].used == 0 {
            snd_wait(self.snd(), &mut snd);
            snd_play(self.snd(), &mut snd, "MNS_01.wav");
            if let Some(name) = self.req_input(ASK_NAME) {
                snd_stop(&mut snd);
                self.create_slot(slot, &name);
                self.mst_print_slot(slot);
                self.set_active_slot(slot);
                self.mst_close_screen();
                self.save_slotdbase();
                return true;
            }
            snd_stop(&mut snd);
        }
        self.activate_form(self.md.mst_form.clone());
        let mut input = SysInput::default();
        let mut running = true;
        while running {
            sys_poll(&mut input);
            let mut r = self.handle_form(&input);
            if (MST_SLOT0..MST_SLOT0 + SLOT_COUNT as i32).contains(&r) {
                let picked = r - MST_SLOT0;
                if picked == slot {
                    // Clicking the selected row again is OK.
                    self.focus_control(MST_OK);
                    r = MST_OK;
                } else {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    self.mst_reselect_slot(slot, picked);
                    slot = picked;
                    snd_wait(self.snd(), &mut snd);
                    continue;
                }
            }
            match r {
                MST_OK => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    if self.slot_list[slot_check_index(slot)].used == 0 {
                        snd_wait(self.snd(), &mut snd);
                        snd_play(self.snd(), &mut snd, "MNS_01.wav");
                        let Some(name) = self.req_input(ASK_NAME) else {
                            snd_stop(&mut snd);
                            self.activate_form(self.md.mst_form.clone());
                            continue;
                        };
                        snd_stop(&mut snd);
                        self.create_slot(slot, &name);
                        self.mst_print_slot(slot);
                    }
                    self.set_active_slot(slot);
                    running = false;
                    snd_wait(self.snd(), &mut snd);
                }
                MST_DELETE => {
                    if self.slot_list[slot_check_index(slot)].used != 0 {
                        snd_play(self.snd(), &mut snd, "click1.wav");
                        snd_wait(self.snd(), &mut snd);
                        snd_play(self.snd(), &mut snd, "MNS_02.wav");
                        let mut q = b"Wil je deze speler '".to_vec();
                        q.extend_from_slice(&self.slot_naam_string(slot));
                        q.extend_from_slice(b"' verwijderen?");
                        if self.req_vraag(&q) {
                            self.clear_slot(slot);
                        }
                        snd_stop(&mut snd);
                        slot = self.get_last_saved_slot();
                        self.mst_print_slot(slot);
                    }
                    self.activate_form(self.md.mst_form.clone());
                }
                MST_HELP => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut snd);
                    let help = self.md.mst_help.clone();
                    self.play_help(&help, 1);
                }
                FORM_ESCAPE | MST_EXIT | FORM_QUIT => {
                    if r == FORM_ESCAPE {
                        self.focus_control(MST_EXIT);
                    }
                    quit = true;
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    running = false;
                    snd_wait(self.snd(), &mut snd);
                }
                FORM_UP | FORM_DOWN => {
                    let next = if r == FORM_UP { (slot + 9) % 10 } else { (slot + 1) % 10 };
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    self.mst_reselect_slot(slot, next);
                    snd_wait(self.snd(), &mut snd);
                    slot = next;
                }
                _ => {}
            }
            xvga_show();
            sys_sleep(10);
        }
        self.mst_close_screen();
        self.save_slotdbase();
        !quit
    }
}
