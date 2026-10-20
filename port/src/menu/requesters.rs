//! Requesters drawn over the dimmed menu: a yes/no question (reqvraag.cpp), a warning
//! (reqwarn.cpp) and a text input (reqinput.cpp).

use crate::gfx::screen::{blt_scr_2_scr, set_mem_image, xvga_show, MI_TOPLEFT};
use crate::menu::forms::*;
use crate::menu::help::draw_grid;
use crate::menu::mysound::{snd_play, Snd};
use crate::menu::txttool::win_printwrapped;
use crate::menu::Menu;
use crate::platform::sys::{keyispressed, sys_poll, sys_sleep, SysInput, SCREEN_H, SCREEN_W};
use crate::xvga::font::{xvga_select_font, xvga_set_font_bkmode, xvga_set_font_colors};
use crate::xvga::sound::sound_buffer_busy;

/// reqv_wait_snd / reqi_wait_snd
fn req_wait_snd(handle: &mut i32) {
    let mut input = SysInput::default();
    while *handle >= 0 && sound_buffer_busy(*handle) {
        sys_poll(&mut input);
        if input.quit {
            break;
        }
        xvga_show();
        sys_sleep(5);
    }
    *handle = -1;
}

impl Menu {
    /// Saves the screen, dims it and puts the requester's panel (anim `panel`) on top.
    fn req_open(&self, panel: i32, text: &[u8]) {
        blt_scr_2_scr(0, 1, 0, 0, SCREEN_W as i32, SCREEN_H as i32, 0);
        draw_grid(0);
        set_mem_image(0, self.anim_image(panel, 0), 0x97, 0x61, MI_TOPLEFT);
        xvga_select_font(1);
        xvga_set_font_colors(1, 0, 0x1efaff);
        win_printwrapped(0, 0x105, 0x7a, 0xcd, text);
    }

    fn req_close(&mut self) {
        blt_scr_2_scr(1, 0, 0, 0, SCREEN_W as i32, SCREEN_H as i32, 0);
        xvga_show();
        wait_form_key_released();
        self.destroy_form();
    }

    /// "Ja"/"Nee": true for yes.
    pub fn req_vraag(&mut self, text: &[u8]) -> bool {
        let snd: Snd = self.snd();
        let mut handle = -1;
        let mut no = false;
        self.req_open(0xf, text);
        self.activate_form(self.md.reqv_form.clone());
        self.focus_control(0);
        let mut input = SysInput::default();
        loop {
            sys_poll(&mut input);
            let mut r = self.handle_form(&input);
            if r == FORM_QUIT || r == 1 || r == FORM_ESCAPE {
                if r == FORM_ESCAPE {
                    self.focus_control(1);
                }
                no = true;
                r = 0;
            }
            if r == 0 {
                snd_play(snd, &mut handle, "click2.wav");
                wait_form_key_released();
                req_wait_snd(&mut handle);
                break;
            }
            xvga_show();
            sys_sleep(10);
        }
        self.req_close();
        !no
    }

    /// A message with an OK button (reqwarn.cpp).
    pub fn req_warning(&mut self, text: &[u8]) -> bool {
        self.req_open(0x11, text);
        self.activate_form(self.md.reqw_form.clone());
        self.focus_control(0);
        let mut input = SysInput::default();
        loop {
            sys_poll(&mut input);
            let r = self.handle_form(&input);
            if r == FORM_ESCAPE {
                self.focus_control(0);
            }
            if r == 0 || r == FORM_ESCAPE || r == FORM_QUIT {
                break;
            }
            xvga_show();
            sys_sleep(10);
        }
        wait_form_key_released();
        self.req_close();
        true
    }

    /// Asks for a line of text; None when cancelled.
    pub fn req_input(&mut self, text: &[u8]) -> Option<Vec<u8>> {
        let snd = self.snd();
        let mut handle = -1;
        let mut result = None;
        self.req_open(0xc, text);
        xvga_select_font(2);
        xvga_set_font_colors(2, 0, 0xf0fafa);
        xvga_set_font_bkmode(2, 2);
        self.activate_form(self.md.reqi_form.clone());
        self.focus_control(2);
        let mut input = SysInput::default();
        loop {
            sys_poll(&mut input);
            let r = self.handle_form(&input);
            if r == 0 {
                // OK
                snd_play(snd, &mut handle, "click2.wav");
                wait_form_key_released();
                result = Some(self.get_edit_box_text(2));
                req_wait_snd(&mut handle);
                break;
            } else if r == FORM_ENTER {
                // Enter in the name field counts as OK.
                self.focus_control(0);
                snd_play(snd, &mut handle, "click2.wav");
                result = Some(self.get_edit_box_text(2));
                while keyispressed(0x0d) {
                    sys_sleep(5);
                }
                break;
            } else if r == 1 || r == FORM_ESCAPE || r == FORM_QUIT {
                if r == FORM_ESCAPE {
                    self.focus_control(1);
                }
                snd_play(snd, &mut handle, "click2.wav");
                wait_form_key_released();
                req_wait_snd(&mut handle);
                break;
            }
            xvga_show();
            sys_sleep(10);
        }
        xvga_set_font_bkmode(2, 1);
        self.req_close();
        result
    }
}
