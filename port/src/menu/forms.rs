//! Forms and their controls (forms.cpp, buttons.cpp, areabuts.cpp, editbox.cpp).
//!
//! The original made each control a Win32 child window: owner-drawn BUTTONs, STATIC
//! windows for clickable areas and EDIT windows for text. Windows gave them focus on a
//! click or Tab, pressed a button on Enter or a click, and sent the form a command. The
//! port keeps that behaviour and draws the controls itself, into the front screen, when
//! their state changes.

use crate::gfx::screen::{blt_scr_2_scr, fill_rect_colorref, get_mem_image_size, set_mem_image, MI_CLIP, SCREEN_FRONT};
use crate::menu::menu_types::*;
use crate::menu::Menu;
use crate::platform::sys::*;
use crate::xvga::font::{get_string_width, win_print_xy, xvga_get_active_font, xvga_select_font, xvga_set_font_bkmode,
    xvga_set_font_colors};

/// handle_form results besides a control index.
pub const FORM_NOTHING: i32 = -1;
/// Window closed.
pub const FORM_QUIT: i32 = -2;
pub const FORM_ESCAPE: i32 = 10000;
pub const FORM_UP: i32 = 10001;
pub const FORM_DOWN: i32 = 10002;
pub const FORM_LEFT: i32 = 10003;
pub const FORM_RIGHT: i32 = 10004;
/// Enter while no button has the focus (e.g. in an edit box).
pub const FORM_ENTER: i32 = 10005;

/// Control states (set_control_state).
pub const STATE_NORMAL: i32 = 0;
pub const STATE_FOCUS: i32 = 1;
pub const STATE_DISABLED: i32 = 2;
/// Table value: normal until drawn.
pub const STATE_DEFAULT: i32 = 3;
pub const STATE_PRESSED: i32 = 4;

const MAX_CONTROLS: usize = 64;
const EDIT_MAX: usize = 256;

pub struct Forms {
    /// The active form's rows; None when no form is active.
    activeform: Option<Vec<Form>>,
    focus: i32,
    pressed: i32,
    state: [i32; MAX_CONTROLS],
    /// Frame last drawn, -1 to force a redraw.
    drawn: [i32; MAX_CONTROLS],
    edit_text: Vec<Vec<u8>>,
    /// The initial text is selected, as when an EDIT gets the focus in a dialog: typing
    /// replaces it.
    edit_selected: [bool; MAX_CONTROLS],
    edit_font: [i32; MAX_CONTROLS],
    focus_anim_time: u32,
    /// allowed_edit_keys: what an edit box accepts (clear_edit_keys_allowed).
    allowed_edit_keys: Vec<u8>,
}

impl Forms {
    pub fn new() -> Forms {
        Forms {
            activeform: None,
            focus: -1,
            pressed: -1,
            state: [0; MAX_CONTROLS],
            drawn: [-1; MAX_CONTROLS],
            edit_text: vec![Vec::new(); MAX_CONTROLS],
            edit_selected: [false; MAX_CONTROLS],
            edit_font: [0; MAX_CONTROLS],
            focus_anim_time: 0,
            allowed_edit_keys: b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890.<,/?![]%$&*+-=>:;' ".to_vec(),
        }
    }

    fn rows(&self) -> &[Form] {
        self.activeform.as_deref().unwrap_or(&[])
    }

    fn count(&self) -> i32 {
        self.rows().len() as i32
    }
}

fn is_edit(f: &Form) -> bool {
    f.ty == CONTROL_EDITBOX || f.ty == CONTROL_EDITBOX_READONLY
}

impl Menu {
    #[allow(dead_code)]
    pub fn set_edit_keys_allowed(&mut self, keys: &[u8]) {
        self.forms.allowed_edit_keys = keys.to_vec();
    }

    /// Sprite index for frame `frame` of anim `anim` (get_control_image_index).
    pub fn get_control_image_index(&self, anim: i32, mut frame: i32) -> i32 {
        let a = &self.md.anims[anim as usize];
        if frame < 0 || frame >= a.frame_count {
            eprintln!("get_control_image_index: anim {anim} has no frame {frame}");
            frame = 0;
        }
        self.anim_image(anim, frame)
    }

    fn button(&self, c: Ctl) -> &Button {
        match c {
            Ctl::Button(t, i) => &self.md.buttons(t)[i],
            _ => panic!("not a button: {c:?}"),
        }
    }

    fn button_mut(&mut self, c: Ctl) -> &mut Button {
        match c {
            Ctl::Button(t, i) => &mut self.md.buttons_mut(t)[i],
            _ => panic!("not a button: {c:?}"),
        }
    }

    fn control_size(&self, f: &Form) -> (i32, i32) {
        match f.control {
            Ctl::Button(t, i) => {
                let b = &self.md.buttons(t)[i];
                (b.width, b.height)
            }
            Ctl::AreaButton(t, i) => {
                let a = &self.md.areabuttons(t)[i];
                (a.width, a.height)
            }
            Ctl::EditBox(t, i) => {
                let e = &self.md.edit_boxes(t)[i];
                (e.width, e.height)
            }
            Ctl::None => (0, 0),
        }
    }

    fn inside(&self, f: &Form, x: i32, y: i32) -> bool {
        let (w, h) = self.control_size(f);
        x >= f.x && y >= f.y && x < f.x + w && y < f.y + h
    }

    // --- buttons ---------------------------------------------------------------------

    fn create_button(&mut self, f: &Form) {
        let image = self.get_control_image_index(self.button(f.control).image, 0);
        let size = get_mem_image_size(image);
        let b = self.button_mut(f.control);
        if let Some((w, h)) = size {
            if b.width == -1 {
                b.width = w;
            }
            if b.height == -1 {
                b.height = h;
            }
        }
        b.focus_frame = 0;
    }

    /// Frames of a button anim: 0 normal, 1 pressed, 2 disabled, 3 focused (draw_button).
    fn button_frame(&self, i: i32) -> i32 {
        let fs = &self.forms;
        if fs.state[i as usize] == STATE_DISABLED {
            2
        } else if i == fs.pressed {
            1
        } else if i == fs.focus {
            3
        } else {
            0
        }
    }

    fn draw_button(&self, i: i32, frame: i32) {
        let f = self.forms.rows()[i as usize];
        let b = self.button(f.control);
        set_mem_image(SCREEN_FRONT, self.get_control_image_index(b.image, frame), f.x + b.width / 2, f.y + b.height / 2, MI_CLIP);
    }

    fn draw_focus_anim_button(&mut self, i: i32) {
        let f = self.forms.rows()[i as usize];
        let b = *self.button(f.control);
        if b.focus_anim == -1 {
            return;
        }
        set_mem_image(SCREEN_FRONT, self.get_control_image_index(b.focus_anim, b.focus_frame), f.x + b.width / 2,
            f.y + b.height / 2, MI_CLIP);
        let count = self.md.anims[b.focus_anim as usize].frame_count;
        let b = self.button_mut(f.control);
        b.focus_frame = if b.focus_frame < count - 1 { b.focus_frame + 1 } else { 0 };
    }

    // --- area buttons ----------------------------------------------------------------

    fn create_areabutton(&mut self, f: &Form) {
        let Ctl::AreaButton(t, i) = f.control else { return };
        let a = self.md.areabuttons(t)[i];
        if a.image == -1 {
            return;
        }
        let image = self.get_control_image_index(a.image, 0);
        let size = get_mem_image_size(image);
        let a = &mut self.md.areabuttons_mut(t)[i];
        if let Some((w, h)) = size {
            if a.width == -1 {
                a.width = w;
            }
            if a.height == -1 {
                a.height = h;
            }
        }
        let (w, h) = (a.width, a.height);
        set_mem_image(SCREEN_FRONT, image, f.x + w / 2, f.y + h / 2, MI_CLIP);
    }

    // --- edit boxes ------------------------------------------------------------------

    fn create_edit_box(&mut self, f: &Form, i: usize) {
        let Ctl::EditBox(t, k) = f.control else { return };
        let e = &mut self.md.edit_boxes_mut(t)[k];
        self.forms.edit_text[i] = e.text.unwrap_or(b"").to_vec();
        self.forms.edit_selected[i] = !self.forms.edit_text[i].is_empty();
        self.forms.edit_font[i] = xvga_get_active_font();
        // create_edit_box's colours: white on black, white on red while focused.
        e.background = 0x000000;
        e.foreground = 0xffffff;
        e.focus_background = 0x0000ff;
        e.focus_foreground = 0xffffff;
    }

    /// The EDIT window: a filled box with the text one pixel in, and a caret when focused.
    fn draw_edit_box(&self, i: usize) {
        let f = self.forms.rows()[i];
        let Ctl::EditBox(t, k) = f.control else { return };
        let e = self.md.edit_boxes(t)[k];
        let focused = i as i32 == self.forms.focus;
        let (bg, fg) = if focused { (e.focus_background, e.focus_foreground) } else { (e.background, e.foreground) };
        let font = xvga_get_active_font();
        let ef = self.forms.edit_font[i];
        let text = &self.forms.edit_text[i];
        xvga_select_font(ef);
        fill_rect_colorref(SCREEN_FRONT, f.x, f.y, e.width, e.height, bg);
        xvga_set_font_colors(ef as usize, bg, fg);
        xvga_set_font_bkmode(ef as usize, 1);
        if focused && self.forms.edit_selected[i] {
            // Selected text: inverted colours.
            let w = get_string_width(text, text.len());
            fill_rect_colorref(SCREEN_FRONT, f.x + 1, f.y + 1, w, e.height - 2, fg);
            xvga_set_font_colors(ef as usize, fg, bg);
        }
        win_print_xy(SCREEN_FRONT, f.x + 1, f.y, text);
        if focused && (sys_ticks() / 500) % 2 == 0 {
            let x = f.x + 1 + get_string_width(text, text.len());
            fill_rect_colorref(SCREEN_FRONT, x, f.y + 2, 1, e.height - 4, fg);
        }
        xvga_select_font(font);
    }

    fn edit_box_input(&mut self, i: usize, input: &SysInput) {
        let f = self.forms.rows()[i];
        let Ctl::EditBox(t, k) = f.control else { return };
        let max_chars = self.md.edit_boxes(t)[k].max_chars;
        let fs = &mut self.forms;
        if fs.edit_selected[i] && (input.backspaces != 0 || !input.text.is_empty()) {
            fs.edit_text[i].clear();
            fs.edit_selected[i] = false;
            if input.text.is_empty() {
                return;
            }
        }
        for _ in 0..input.backspaces {
            fs.edit_text[i].pop();
        }
        for &c in &input.text {
            // EditWindowProc only lets allowed_edit_keys through.
            let n = fs.edit_text[i].len();
            if !fs.allowed_edit_keys.contains(&c) || n as i32 >= max_chars || n + 1 >= EDIT_MAX {
                continue;
            }
            fs.edit_text[i].push(c);
        }
    }

    pub fn get_edit_box_text(&self, index: i32) -> Vec<u8> {
        let rows = self.forms.rows();
        assert!(index >= 0 && index < rows.len() as i32 && is_edit(&rows[index as usize]),
            "get_edit_box_text: control {index} is not an edit box");
        self.forms.edit_text[index as usize].clone()
    }

    // --- forms -----------------------------------------------------------------------

    fn redraw_changed(&mut self, force: bool) {
        for i in 0..self.forms.count() {
            let f = self.forms.rows()[i as usize];
            if f.ty == CONTROL_BUTTON {
                let frame = self.button_frame(i);
                if force || frame != self.forms.drawn[i as usize] {
                    self.draw_button(i, frame);
                    self.forms.drawn[i as usize] = frame;
                }
            } else if is_edit(&f) {
                self.draw_edit_box(i as usize);
            }
        }
    }

    /// Makes a form table the active form (its rows up to the one whose type is -1).
    pub fn activate_form(&mut self, form: Vec<Form>) {
        if self.forms.activeform.is_some() {
            self.destroy_form();
        }
        let rows: Vec<Form> = form.into_iter().take_while(|f| f.ty != -1).take(MAX_CONTROLS).collect();
        self.forms.focus = -1;
        self.forms.pressed = -1;
        for (i, f) in rows.iter().enumerate() {
            if f.ty == CONTROL_BUTTON {
                self.create_button(f);
            } else if f.ty == CONTROL_AREABUTTON {
                self.create_areabutton(f);
            } else if is_edit(f) {
                self.create_edit_box(f, i);
            }
            // set_control_state: 4 becomes 1; state 1 takes the focus.
            let st = if f.state == STATE_PRESSED { STATE_FOCUS } else { f.state };
            self.forms.state[i] = st;
            if st == STATE_FOCUS && self.forms.focus < 0 {
                self.forms.focus = i as i32;
            }
            self.forms.drawn[i] = -1;
        }
        let focus_edit = self.forms.focus >= 0 && is_edit(&rows[self.forms.focus as usize]);
        self.forms.activeform = Some(rows);
        if focus_edit {
            sys_text_input(true);
        }
        self.redraw_changed(true);
        self.forms.focus_anim_time = sys_ticks();
    }

    pub fn destroy_form(&mut self) {
        sys_text_input(false);
        self.forms.activeform = None;
    }

    pub fn get_control_focus(&self) -> i32 {
        self.forms.focus
    }

    #[allow(dead_code)]
    pub fn get_control_type(&self, index: i32) -> i32 {
        self.forms.rows().get(index as usize).map(|f| f.ty).unwrap_or(-1)
    }

    pub fn get_numberofcontrols_inform(&self) -> i32 {
        self.forms.count()
    }

    pub fn focus_control(&mut self, index: i32) {
        if index >= 0 && index < self.forms.count() && self.forms.state[index as usize] != STATE_DISABLED {
            self.forms.focus = index;
            sys_text_input(is_edit(&self.forms.rows()[index as usize]));
            self.redraw_changed(false);
        }
    }

    /// set_next_control_focus / set_previous_control_focus (Tab, Shift+Tab).
    fn step_focus(&mut self, dir: i32) {
        let n = self.forms.count();
        for k in 1..=n {
            let i = ((self.forms.focus.max(0) + dir * k) % n + n) % n;
            if self.forms.state[i as usize] != STATE_DISABLED && self.forms.rows()[i as usize].ty != CONTROL_AREABUTTON {
                self.focus_control(i);
                return;
            }
        }
    }

    /// draw_help_button & co.: the control drawn undimmed while help talks about it.
    pub fn call_help_function(&mut self, index: i32) {
        let Some(&f) = self.forms.rows().get(index as usize) else { return };
        if f.ty == CONTROL_BUTTON {
            let st = self.forms.state[index as usize];
            self.draw_button(index, if st == STATE_FOCUS || st == STATE_PRESSED { 1 } else { 0 });
            self.forms.drawn[index as usize] = -1;
        } else {
            let (w, h) = self.control_size(&f);
            blt_scr_2_scr(2, 0, f.x, f.y, f.x + w, f.y + h, 0);
        }
    }

    /// Draws the active form.
    pub fn draw_form(&mut self, _which: usize) {
        self.redraw_changed(true);
    }

    /// Processes input once and redraws the form; a control index, FORM_* or FORM_NOTHING.
    pub fn handle_form(&mut self, input: &SysInput) -> i32 {
        if self.forms.activeform.is_none() {
            return FORM_NOTHING;
        }
        if input.quit {
            return FORM_QUIT;
        }
        let rows = self.forms.rows().to_vec();
        let mut under = -1;
        for (i, f) in rows.iter().enumerate() {
            if self.forms.state[i] != STATE_DISABLED && self.inside(f, input.mouse_x, input.mouse_y) {
                under = i as i32;
            }
        }
        // A button press takes the focus and shows the pressed frame until release.
        if (input.mouse_down || input.mouse_pressed) && under >= 0 && self.forms.pressed < 0 {
            let ty = rows[under as usize].ty;
            self.forms.pressed = if ty == CONTROL_BUTTON { under } else { -1 };
            if ty != CONTROL_AREABUTTON {
                self.focus_control(under);
            }
        }
        let mut clicked = -1;
        if input.mouse_clicked {
            if under >= 0 && (self.forms.pressed == under || rows[under as usize].ty == CONTROL_AREABUTTON) {
                clicked = under;
            }
            self.forms.pressed = -1;
        }
        if !input.mouse_down {
            self.forms.pressed = -1;
        }
        let focus = self.forms.focus;
        if focus >= 0 && is_edit(&rows[focus as usize]) {
            self.edit_box_input(focus as usize, input);
        }
        self.redraw_changed(false);

        // draw_form_animfocus_buttons, every form_anim_timer calls.
        if focus >= 0 && rows[focus as usize].ty == CONTROL_BUTTON && sys_ticks().wrapping_sub(self.forms.focus_anim_time) >= 66 {
            self.forms.focus_anim_time = sys_ticks();
            self.draw_focus_anim_button(focus);
        }

        if clicked >= 0 && !is_edit(&rows[clicked as usize]) {
            return clicked;
        }
        // check_form_key_pressed
        let focus = self.forms.focus;
        let focus_is_button = focus >= 0 && rows[focus as usize].ty == CONTROL_BUTTON;
        match input.key {
            KEY_ESCAPE => return FORM_ESCAPE,
            KEY_UP => return FORM_UP,
            KEY_DOWN => return FORM_DOWN,
            KEY_LEFT => return FORM_LEFT,
            KEY_RIGHT => return FORM_RIGHT,
            9 => {
                self.step_focus(if keyispressed(0x10) { -1 } else { 1 });
                return FORM_NOTHING;
            }
            // Enter presses the focused button; in an edit box the requester checks it.
            KEY_ENTER => return if focus_is_button { focus } else { FORM_ENTER },
            KEY_SPACE if focus_is_button => return focus,
            _ => {}
        }
        // Alt + hotkey presses the control with that hotkey.
        if keyispressed(0x12) && input.key > 0 && input.key < 128 {
            for (i, f) in rows.iter().enumerate() {
                let hotkey = match f.control {
                    Ctl::Button(t, k) if f.ty == CONTROL_BUTTON => self.md.buttons(t)[k].hotkey,
                    Ctl::AreaButton(t, k) if f.ty == CONTROL_AREABUTTON => self.md.areabuttons(t)[k].hotkey,
                    _ => 0,
                };
                if hotkey > b' ' && (hotkey | 0x20) as i32 == (input.key | 0x20) {
                    return i as i32;
                }
            }
        }
        FORM_NOTHING
    }
}

/// wait_form_key_released: until the mouse button and Enter are up.
pub fn wait_form_key_released() {
    let mut input = SysInput::default();
    loop {
        sys_poll(&mut input);
        sys_sleep(5);
        if !((input.mouse_down || keyispressed(0x0d) || keyispressed(0x20)) && !input.quit) {
            break;
        }
    }
}
