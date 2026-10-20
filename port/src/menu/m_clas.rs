//! The overall classification after a race (m_clas.cpp): the player and the five
//! rivals by total time over the stages driven so far, over TStand.pcx (TStand2.pcx
//! after the last stage).

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, clear_screen, xvga_show};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::menu_data::SPELER_NAMEN;
use crate::menu::menu_types::cstr;
use crate::menu::menu_util::{load_pcx_file_1, time_string};
use crate::menu::slot::slot_check_index;
use crate::menu::Menu;
use crate::xvga::font::{win_print_xy, xvga_select_font, xvga_set_font_colors};

/// mdc_stand_list's entries (9 bytes each in the original).
struct MdcStand {
    time: i32,
    name: Vec<u8>,
    is_player: bool,
}

impl Menu {
    fn mdc_create_huidige_stand(&self) -> Vec<MdcStand> {
        let s = &self.slot_list[slot_check_index(self.active_slot)];
        let mut list = vec![MdcStand {
            time: self.slot_count_total_tijd(self.active_slot, s.stage as i32),
            name: cstr(&s.name).to_vec(),
            is_player: true,
        }];
        for (r, name) in SPELER_NAMEN.iter().enumerate().take(5) {
            list.push(MdcStand {
                time: self.slot_count_total_tijd_speler(self.active_slot, r),
                name: name.to_vec(),
                is_player: false,
            });
        }
        // Shortest time first; equal times by name.
        list.sort_by(|p, q| p.time.cmp(&q.time).then_with(|| p.name.cmp(&q.name)));
        list
    }

    fn mdc_display_huidige_stand(list: &[MdcStand], dx: i32, dy: i32, shadow: bool) {
        let (normal, player) = if shadow { (0, 0) } else { (0xafaff, 0xa0fff) };
        let mut y = dy + 0x9b;
        xvga_select_font(2);
        for (i, e) in list.iter().enumerate() {
            let c = if e.is_player { player } else { normal };
            xvga_set_font_colors(2, c, c);
            win_print_xy(1, dx + 0x2c, y, (i + 1).to_string().as_bytes());
            win_print_xy(1, dx + 0x52, y, &e.name);
            win_print_xy(1, dx + 0x1e4, y, &time_string(e.time));
            y += 0x19;
        }
    }

    fn mdc_display_score(&self) {
        let list = self.mdc_create_huidige_stand();
        xvga_select_font(2);
        blt_scr_2_scr(2, 1, 0x2c, 0x9b, 0x270, 0x195, 0);
        Self::mdc_display_huidige_stand(&list, 2, 2, true);
        Self::mdc_display_huidige_stand(&list, 1, 1, true);
        Self::mdc_display_huidige_stand(&list, 0, 0, false);
        blt_scr_2_scr(1, 0, 0x2c, 0x9b, 0x270, 0x195, 0);
        xvga_show();
    }

    /// Shows the player and the five rivals ordered by their total time so far.
    pub fn menu_display_classe(&mut self, g: &Game) {
        let mut snd = -1;
        let stage = self.slot_list[slot_check_index(self.active_slot)].stage;
        load_pcx_file_1(g, 2, if stage < 6 { "TStand.pcx" } else { "TStand2.pcx" }, 0x1f);
        clear_screen(0, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.mdc_display_score();
        self.activate_form(self.md.mdc_form.clone());
        self.focus_control(0);
        self.wait_back_button(&mut snd);
        clear_screen(2, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
    }
}
