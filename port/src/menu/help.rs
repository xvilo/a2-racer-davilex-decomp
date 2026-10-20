//! Spoken help (help.cpp): the screen is dimmed with a grid and each control is shown
//! while its voice clip plays.

use crate::gfx::screen::{blt_scr_2_scr, with_screen, xvga_show};
use crate::menu::menu_types::Help;
use crate::menu::Menu;
use crate::platform::sys::{keyispressed, sys_poll, sys_sleep, SysInput, SCREEN_H, SCREEN_W};
use crate::xvga::sound::{play_sound_buffer_1, sound_buffer_busy, stop_sound_buffer};

const MAX_HELP_KEYS: usize = 8;

/// Dims a screen with the 8x8 helpgrid surface: every other pixel in a checkerboard
/// becomes palette index 0 (0xff, its colour key, leaves the pixel alone).
pub fn draw_grid(which: usize) {
    with_screen(|s| {
        for y in 0..SCREEN_H {
            for x in 0..SCREEN_W {
                if (x + y) % 2 == 1 {
                    s.pixels[which][y * SCREEN_W + x] = 0;
                }
            }
        }
    });
}

impl Menu {
    pub fn set_help_key(&mut self, vk: i32) {
        if self.help_keys.len() < MAX_HELP_KEYS {
            self.help_keys.push(vk);
        }
    }

    pub fn clear_help_keys(&mut self) {
        self.help_keys.clear();
    }

    fn check_help_keys(&self) -> bool {
        self.help_keys.iter().any(|&k| keyispressed(k))
    }

    /// Plays the help for the active form; `save` is the screen that keeps the picture.
    pub fn play_help(&mut self, help: &[Help], save: usize) {
        let n = self.get_numberofcontrols_inform();
        let mut buffer = -1;
        if n == 0 {
            eprintln!("play_help: empty form");
            return;
        }
        blt_scr_2_scr(0, save, 0, 0, SCREEN_W as i32, SCREEN_H as i32, 0);
        // Wait until the key that asked for help is up, or it would end the help at once.
        while self.check_help_keys() {
            sys_sleep(5);
        }
        'help: for h in help.iter().take_while(|h| h.control_id != -1) {
            draw_grid(0);
            if h.control_id < 0 || h.control_id >= n {
                eprintln!("play_help: illegal control {}", h.control_id);
                continue;
            }
            let name = h.text.map(|t| String::from_utf8_lossy(t).into_owned()).unwrap_or_default();
            buffer = play_sound_buffer_1(&name, 0);
            self.call_help_function(h.control_id);
            xvga_show();
            let mut input = SysInput::default();
            while sound_buffer_busy(buffer) {
                sys_poll(&mut input);
                if self.check_help_keys() || input.quit {
                    break 'help;
                }
                xvga_show();
                sys_sleep(10);
            }
        }
        stop_sound_buffer(buffer);
        blt_scr_2_scr(save, 0, 0, 0, SCREEN_W as i32, SCREEN_H as i32, 0);
        xvga_show();
        while self.check_help_keys() {
            sys_sleep(5);
        }
        self.draw_form(0);
    }
}
