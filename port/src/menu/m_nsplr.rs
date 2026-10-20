//! The new-player screen (m_nsplr.cpp): from the main menu, switch to another player or
//! create one. Cancelling puts the slots back as they were (OPSTART.TMP).

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::a2types::SLOT_COUNT;
use crate::menu::forms::*;
use crate::menu::m_start::ASK_NAME;
use crate::menu::menu_util::{load_pcx_file_1, num_right, pad_right};
use crate::menu::mysound::{snd_play, snd_set_volume, snd_stop, snd_volume_for_level, snd_wait};
use crate::menu::slot::{slot_check_index, slots_bytes};
use crate::menu::txttool::win_bprintf;
use crate::menu::Menu;
use crate::platform::sys::{sys_poll, sys_sleep, SysInput};
use crate::xvga::fileio::{file_delete, file_write};
use crate::xvga::font::{win_print_xy, xvga_select_font, xvga_set_font_colors};

// Controls of mns_form, by index; 4..13 are the ten slot rows.
const MNS_OK: i32 = 0;
const MNS_DELETE: i32 = 1;
const MNS_CANCEL: i32 = 2;
const MNS_HELP: i32 = 3;
const MNS_SLOT0: i32 = 4;

impl Menu {
    /// One row, drawn into the buffer screen: plain (anim 0 frame 0) or selected (frame 1).
    fn mns_draw_row(&self, slot: i32, selected: bool) {
        let p = self.md.mns_slot_list[slot as usize];
        set_mem_image(2, self.anim_image(0, selected as i32), p.x, p.y, MI_TOPLEFT);
        xvga_select_font(2);
        let c = if selected { 0xfafafa } else { 0xafaff };
        xvga_set_font_colors(2, c, c);
        win_bprintf(2, p.x + 10, p.y, 0x80, &pad_right(&self.slot_naam_string(slot), 16));
        win_print_xy(2, p.x + 0x94, p.y, &pad_right(&self.slot_level_string(slot), 16));
        win_print_xy(2, p.x + 0xfe, p.y, &num_right(self.slot_list[slot as usize].money, 9));
        win_print_xy(2, p.x + 0x189, p.y, &pad_right(&self.slot_tijd_string(slot), 6));
    }

    fn mns_show(&self) {
        blt_scr_2_scr(2, 0, 0x42, 0x8d, 0x23b, 0x17d, 0);
        xvga_show();
    }

    fn mns_print_slot(&self, selected: i32) {
        for i in 0..SLOT_COUNT as i32 {
            self.mns_draw_row(i, false);
        }
        self.mns_draw_row(selected, true);
        self.mns_show();
    }

    fn mns_reselect_slot(&self, old_slot: i32, new_slot: i32) {
        self.mns_draw_row(old_slot, false);
        self.mns_draw_row(new_slot, true);
        self.mns_show();
    }

    fn mns_music(&mut self, level: i16) {
        snd_set_volume(self.snd(), &mut self.mm_tune_buffer, snd_volume_for_level(level));
    }

    /// True when a player was picked, false when cancelled (slots restored).
    pub fn menu_nieuwesplr(&mut self, g: &Game) -> bool {
        let mut snd = -1;
        let mut cancel = false;
        let saved = self.slot_list; // save_tmp_slotdbase: OPSTART.TMP
        file_write(&slots_bytes(&self.slot_list), "OPSTART.TMP");
        let mut slot = self.get_last_saved_slot();
        load_pcx_file_1(g, 2, "NSPEL_2.PCX", 0x1f);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.mns_print_slot(slot);
        self.activate_form(self.md.mns_form.clone());
        let mut input = SysInput::default();
        let mut running = true;
        while running {
            sys_poll(&mut input);
            let mut r = self.handle_form(&input);
            if (MNS_SLOT0..MNS_SLOT0 + SLOT_COUNT as i32).contains(&r) {
                let picked = r - MNS_SLOT0;
                if picked == slot {
                    self.focus_control(MNS_OK);
                    r = MNS_OK;
                } else {
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    self.mns_reselect_slot(slot, picked);
                    slot = picked;
                    snd_wait(self.snd(), &mut snd);
                    continue;
                }
            }
            match r {
                MNS_OK => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    let mut ok = true;
                    if self.slot_list[slot_check_index(slot)].used == 0 {
                        snd_wait(self.snd(), &mut snd);
                        self.mns_music(self.opties.music_volume / 2);
                        snd_play(self.snd(), &mut snd, "MNS_01.wav");
                        let name = self.req_input(ASK_NAME);
                        snd_stop(&mut snd);
                        self.mns_music(self.opties.music_volume);
                        match name {
                            Some(name) => {
                                self.create_slot(slot, &name);
                                self.mns_print_slot(slot);
                            }
                            None => {
                                self.activate_form(self.md.mns_form.clone());
                                ok = false;
                            }
                        }
                    }
                    if ok {
                        self.set_active_slot(slot);
                        running = false;
                        snd_wait(self.snd(), &mut snd);
                    }
                }
                MNS_DELETE => {
                    if self.slot_list[slot_check_index(slot)].used != 0 {
                        snd_play(self.snd(), &mut snd, "click1.wav");
                        snd_wait(self.snd(), &mut snd);
                        self.mns_music(self.opties.music_volume / 2);
                        snd_play(self.snd(), &mut snd, "MNS_02.wav");
                        let mut q = b"Wil je deze speler '".to_vec();
                        q.extend_from_slice(&self.slot_naam_string(slot));
                        q.extend_from_slice(b"' verwijderen?");
                        if self.req_vraag(&q) {
                            self.clear_slot(slot);
                        }
                        snd_stop(&mut snd);
                        self.mns_music(self.opties.music_volume);
                        slot = self.get_last_saved_slot();
                        self.mns_print_slot(slot);
                    }
                    self.activate_form(self.md.mns_form.clone());
                }
                FORM_ESCAPE | FORM_QUIT | MNS_CANCEL => {
                    if r != MNS_CANCEL {
                        self.focus_control(MNS_CANCEL);
                    }
                    cancel = true;
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    running = false;
                    snd_wait(self.snd(), &mut snd);
                }
                MNS_HELP => {
                    snd_play(self.snd(), &mut snd, "click1.wav");
                    wait_form_key_released();
                    snd_wait(self.snd(), &mut snd);
                    snd_set_volume(self.snd(), &mut self.mm_tune_buffer, -8000);
                    let help = self.md.mns_help.clone();
                    self.play_help(&help, 1);
                    self.mns_music(self.opties.music_volume);
                }
                FORM_UP | FORM_DOWN => {
                    let next = if r == FORM_UP { (slot + 9) % 10 } else { (slot + 1) % 10 };
                    snd_play(self.snd(), &mut snd, "click3.wav");
                    self.mns_reselect_slot(slot, next);
                    snd_wait(self.snd(), &mut snd);
                    slot = next;
                }
                _ => {}
            }
            xvga_show();
            sys_sleep(10);
        }
        wait_form_key_released();
        self.destroy_form();
        if cancel {
            self.slot_list = saved; // load_tmp_slotdbase
        }
        file_delete("OPSTART.TMP");
        self.save_slotdbase();
        !cancel
    }
}
