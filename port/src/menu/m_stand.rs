//! The stage result after a race (m_stand.cpp): positions 1 to 6 of the stage just
//! driven, with name and time, over that stage's picture (Etappe1..6.pcx).

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, clear_screen, xvga_show};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::menu_data::{MDT_STAND_PCX, SPELER_NAMEN};
use crate::menu::menu_types::cstr;
use crate::menu::menu_util::{load_pcx_file_1, time_string};
use crate::menu::slot::{slot_check_index, slot_check_level};
use crate::menu::Menu;
use crate::xvga::font::{win_print_xy, xvga_select_font, xvga_set_font_colors};

impl Menu {
    /// One pass of the table at an offset; passes at (2,2) and (1,1) in black, then
    /// (0,0) in colour, give the shadow. The player's row is drawn in another colour.
    fn mdt_display_stand(&self, stage: i32, dx: i32, dy: i32, shadow: bool) {
        let (normal, player) = if shadow { (0, 0) } else { (0xafaff, 0xa0fff) };
        if stage < 0 {
            return;
        }
        xvga_select_font(2);
        let s = &self.slot_list[slot_check_index(self.active_slot)];
        let level = slot_check_level(stage);
        let mut y = dy + 0x9b;
        for pos in 1..7 {
            let mut c = normal;
            let mut name: &[u8] = b"";
            let mut time = 0;
            if s.position[level] == pos {
                c = player;
                name = cstr(&s.name);
                time = s.time[level];
            }
            for r in 0..5 {
                if s.racer[r][level] == pos {
                    name = SPELER_NAMEN[r];
                    time = s.time_racer[r][level];
                }
            }
            xvga_set_font_colors(2, c, c);
            win_print_xy(1, dx + 0x2c, y, pos.to_string().as_bytes());
            win_print_xy(1, dx + 0x52, y, name);
            win_print_xy(1, dx + 0x1e4, y, &time_string(time));
            y += 0x19;
        }
    }

    fn mdt_display_score(&self, stage: i32) {
        xvga_select_font(2);
        blt_scr_2_scr(2, 1, 0x2c, 0x9b, 0x270, 0x195, 0);
        self.mdt_display_stand(stage, 2, 2, true);
        self.mdt_display_stand(stage, 1, 1, true);
        self.mdt_display_stand(stage, 0, 0, false);
        blt_scr_2_scr(1, 0, 0x2c, 0x9b, 0x270, 0x195, 0);
        xvga_show();
    }

    /// Shows the finishing order of `stage` for the active player.
    pub fn menu_display_stand(&mut self, g: &Game, stage: i32) {
        let mut snd = -1;
        load_pcx_file_1(g, 2, &String::from_utf8_lossy(MDT_STAND_PCX[(stage % 6) as usize]), 0x1f);
        clear_screen(0, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.mdt_display_score(stage);
        self.activate_form(self.md.mdt_form.clone());
        self.focus_control(0);
        self.wait_back_button(&mut snd);
        clear_screen(2, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
    }
}
