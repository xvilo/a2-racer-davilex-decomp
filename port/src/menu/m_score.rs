//! The score screens: menu_score with its tabs (m_score.cpp) and menu_display_score,
//! shown after a race (m_dscore.cpp).

use crate::game::Game;
use crate::gfx::screen::{blt_scr_2_scr, clear_screen, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::gfx::wipes::{handle_wipe, WIPE_PIXELFADE};
use crate::menu::forms::*;
use crate::menu::menu_data::LEVEL_NAAM;
use crate::menu::menu_types::{cstr, SingleScore};
use crate::menu::menu_util::{load_pcx_file_1, num_right, time_string};
use crate::menu::mysound::{snd_play, snd_wait};
use crate::menu::txttool::win_bprintf;
use crate::menu::Menu;
use crate::platform::sys::{sys_poll, sys_sleep, SysInput};
use crate::xvga::font::{win_print_xy, xvga_select_font, xvga_set_font_colors};

// Controls of ms_form: 0 back ("Terug"), 1 the overall list, 2..7 stages 1..6.
const MS_BACK: i32 = 0;
const MS_OVERALL: i32 = 1;
const MS_STAGE1: i32 = 2;

fn ms_get_current_display(stage: i32) -> i32 {
    if (0..=5).contains(&stage) { stage } else { -1 }
}

/// How the four print functions of the original lay out a table.
struct ScoreLayout {
    /// First row and row height.
    top: i32,
    row: i32,
    /// The entry added last; others are 0xafaff.
    new_color: u32,
    /// The rank right-aligned in two places ("%2d") or not ("%d").
    rank_width: usize,
    name_width: i32,
    /// Stage name column, -1 for none.
    level_x: i32,
    /// -1 for none.
    money_x: i32,
    /// > 0: shortened to fit (win_bprintf).
    money_width: i32,
}

const MS_SCORE: ScoreLayout = ScoreLayout { top: 0xb1, row: 0x17, new_color: 0xa0ffa, rank_width: 2, name_width: 0xb0, level_x: -1, money_x: 0x10c, money_width: 0 };
const MS_HISCORE: ScoreLayout = ScoreLayout { top: 0xb1, row: 0x17, new_color: 0xa0ffa, rank_width: 0, name_width: 0x84, level_x: 0xe0, money_x: 0x162, money_width: 0 };
const MDS_SCORE: ScoreLayout = ScoreLayout { top: 0x9b, row: 0x19, new_color: 0xa0fff, rank_width: 2, name_width: 0x84, level_x: -1, money_x: 0x10c, money_width: 0x96 };
const MDS_HISCORE: ScoreLayout = ScoreLayout { top: 0x9b, row: 0x19, new_color: 0xa0fff, rank_width: 2, name_width: 0x84, level_x: 0xe0, money_x: -1, money_width: 0 };

/// Ten rows at an offset; passes at (2,2) and (1,1) in black, then (0,0) in colour,
/// give the shadow.
fn print_scores(list: &[SingleScore], l: &ScoreLayout, d: i32, shadow: bool) {
    let (normal, fresh) = if shadow { (0, 0) } else { (0xafaff, l.new_color) };
    let mut y = d + l.top;
    for (i, s) in list.iter().enumerate() {
        let c = if s.is_new != 0 { fresh } else { normal };
        xvga_set_font_colors(2, c, c);
        win_print_xy(1, d + 0x2c, y, &num_right(i as i32 + 1, l.rank_width));
        win_bprintf(1, d + 0x52, y, l.name_width, cstr(&s.name));
        if l.level_x >= 0 {
            win_print_xy(1, d + l.level_x, y, LEVEL_NAAM[s.stage as usize]);
        }
        if l.money_x >= 0 && l.money_width > 0 {
            win_bprintf(1, d + l.money_x, y, l.money_width, &num_right(s.money, 9));
        } else if l.money_x >= 0 {
            win_print_xy(1, d + l.money_x, y, &num_right(s.money, 9));
        }
        win_print_xy(1, d + 0x1e4, y, &time_string(s.time));
        y += l.row;
    }
}

impl Menu {
    fn score_list_for(&self, display: i32) -> Vec<SingleScore> {
        if display < 0 { self.hiscore.to_vec() } else { self.levelscore[display as usize].to_vec() }
    }

    // --- menu_score (m_score.cpp) ----------------------------------------------------

    fn ms_display_score(&self, display: i32) {
        xvga_select_font(2);
        blt_scr_2_scr(2, 1, 0x2c, 0xb1, 0x25c, 0x197, 0);
        let list = self.score_list_for(display);
        for pass in (0..=2).rev() {
            print_scores(&list, if display < 0 { &MS_HISCORE } else { &MS_SCORE }, pass, pass > 0);
        }
        // The tab row (anim 0x50: stages 0..5, 6 overall) and the column heads (anim 0x51).
        set_mem_image(0, self.anim_image(0x50, if display < 0 { 6 } else { display }), 0, 0x70, MI_TOPLEFT);
        set_mem_image(0, self.anim_image(0x51, if display < 0 { 0 } else { 1 }), 0, 0x99, MI_TOPLEFT);
        blt_scr_2_scr(1, 0, 0x2c, 0xb1, 0x25c, 0x197, 0);
        xvga_show();
    }

    /// The score menu, opened on a stage's list (0..5) or the overall list (-1).
    pub fn menu_score(&mut self, g: &Game, stage: i32) -> i32 {
        let mut snd = -1;
        let mut display = ms_get_current_display(stage);
        load_pcx_file_1(g, 2, "Score.pcx", 0x1f);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.ms_display_score(display);
        self.activate_form(self.md.ms_form.clone());
        self.focus_control(MS_BACK);
        let mut input = SysInput::default();
        loop {
            sys_poll(&mut input);
            let mut r = self.handle_form(&input);
            if r == FORM_ESCAPE || r == FORM_QUIT {
                self.focus_control(MS_BACK);
                r = MS_BACK;
            }
            if r == MS_BACK {
                snd_play(self.snd(), &mut snd, "click1.wav");
                snd_wait(self.snd(), &mut snd);
                break;
            }
            let pick = if r == MS_OVERALL {
                Some(-1)
            } else if (MS_STAGE1..MS_STAGE1 + 6).contains(&r) {
                Some(r - MS_STAGE1)
            } else {
                None
            };
            if let Some(p) = pick.filter(|&p| p != display) {
                display = p;
                snd_play(self.snd(), &mut snd, "click3.wav");
                wait_form_key_released();
                self.ms_display_score(display);
                snd_wait(self.snd(), &mut snd);
            }
            xvga_show();
            sys_sleep(10);
        }
        wait_form_key_released();
        self.destroy_form();
        0
    }

    // --- menu_display_score (m_dscore.cpp) ---------------------------------------------

    fn mds_display_score(&self, display: i32) {
        xvga_select_font(2);
        blt_scr_2_scr(2, 1, 0x2c, 0x9b, 0x270, 0x195, 0);
        let list = self.score_list_for(display);
        for pass in (0..=2).rev() {
            print_scores(&list, if display < 0 { &MDS_HISCORE } else { &MDS_SCORE }, pass, pass > 0);
        }
        blt_scr_2_scr(1, 0, 0x2c, 0x9b, 0x270, 0x195, 0);
        xvga_show();
    }

    /// The list shown after a race, with only a back button.
    pub fn menu_display_score(&mut self, g: &Game, stage: i32) {
        let mut snd = -1;
        let display = ms_get_current_display(stage);
        load_pcx_file_1(g, 2, if display == -1 { "Score3.pcx" } else { "Score2.pcx" }, 0x1f);
        clear_screen(0, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
        self.mds_display_score(display);
        self.activate_form(self.md.mds_form.clone());
        self.focus_control(0);
        self.wait_back_button(&mut snd);
        clear_screen(2, 0);
        handle_wipe(2, WIPE_PIXELFADE, 600);
    }

    /// The loop of the screens after a race: until their one button (or Escape).
    pub fn wait_back_button(&mut self, snd: &mut i32) {
        let mut input = SysInput::default();
        loop {
            sys_poll(&mut input);
            let r = self.handle_form(&input);
            if r == FORM_ESCAPE || r == FORM_QUIT {
                self.focus_control(0);
            }
            if r == 0 || r == FORM_ESCAPE || r == FORM_QUIT {
                snd_play(self.snd(), snd, "click1.wav");
                snd_wait(self.snd(), snd);
                break;
            }
            xvga_show();
            sys_sleep(10);
        }
        wait_form_key_released();
        self.destroy_form();
    }
}
