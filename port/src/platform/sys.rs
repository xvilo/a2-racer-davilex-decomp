//! Platform layer: window, presenting the 8-bit screen, input and time. Replaces the
//! Win32 message loop and DirectDraw/DirectInput parts of the xvga library.
//!
//! Scripted input for tests: one command per line, run in order as time passes.
//!   wait <ms>         pause the script
//!   hold <name> <ms>  hold a key down for a while (for keyispressed)
//!   key <name>        press and release a key (enter, escape, up, down, left, right,
//!                     tab, space, backspace, f1..f12, or a letter/digit; alt+<x> holds Alt)
//!   text <string>     type text
//!   click <x> <y>     left click at a 640x480 position
//!   shot <file.png>   save the front screen
//!   glshot <file.png> save the window's picture as drawn
//!   quit              close the window

use std::cell::RefCell;
use std::ffi::c_void;
use std::path::{Path, PathBuf};

use sdl2::event::Event;
use sdl2::joystick::Joystick;
use sdl2::keyboard::{Keycode, Mod, Scancode};
use sdl2::mouse::{MouseButton, MouseState};
use sdl2::video::{GLContext, GLProfile, Window};
use sdl2::{EventPump, EventSubsystem, JoystickSubsystem, Sdl, VideoSubsystem};

use crate::gfx::png::png_write_rgb;
use crate::platform::gl::*;

pub const SCREEN_W: usize = 640;
pub const SCREEN_H: usize = 480;

pub const KEY_NONE: i32 = 0;
pub const KEY_ESCAPE: i32 = 27;
pub const KEY_ENTER: i32 = 13;
pub const KEY_SPACE: i32 = 32;
pub const KEY_UP: i32 = 0x100;
pub const KEY_DOWN: i32 = 0x101;
pub const KEY_LEFT: i32 = 0x102;
pub const KEY_RIGHT: i32 = 0x103;

#[derive(Default, Clone)]
pub struct SysInput {
    /// In 640x480 screen coordinates.
    pub mouse_x: i32,
    pub mouse_y: i32,
    /// Left button held.
    pub mouse_down: bool,
    /// Left button went down since the last poll.
    pub mouse_pressed: bool,
    /// Left button released since the last poll.
    pub mouse_clicked: bool,
    /// Last key pressed since the last poll, KEY_* or ASCII.
    pub key: i32,
    /// Characters typed since the last poll (Windows-1252).
    pub text: Vec<u8>,
    /// Backspace presses since the last poll.
    pub backspaces: i32,
    /// Window closed.
    pub quit: bool,
}

pub struct SysOptions {
    /// Initial window size = 640x480 * scale.
    pub scale: u32,
    /// No window (screenshots, tests).
    pub hidden: bool,
    /// Scripted input, see the module comment (tests).
    pub script: Option<PathBuf>,
}

struct Script {
    lines: Vec<String>,
    pos: usize,
    resume: u32,
    fake_alt: bool,
    fake_key: Option<Scancode>,
    fake_key_until: u32,
    held: Vec<(Scancode, u32)>,
    glshot: Option<PathBuf>,
}

struct Platform {
    sdl: Sdl,
    video: VideoSubsystem,
    window: Window,
    _gl_context: GLContext,
    events: EventPump,
    /// Kept for the whole run: dropping the last handle of an SDL subsystem shuts it down.
    event_subsystem: EventSubsystem,
    joystick_subsystem: Option<JoystickSubsystem>,
    joysticks: [Option<Joystick>; 2],
    screen_texture: GLuint,
    rgba: Vec<u8>,
    /// Mouse state survives across inputs: the game polls from many nested loops.
    mouse: (i32, i32, bool),
    script: Script,
}

thread_local! {
    static PLATFORM: RefCell<Option<Platform>> = const { RefCell::new(None) };
    /// The screen the script's "shot" command saves; set by the game.
    static SCREENSHOT_FN: RefCell<Option<fn(&Path)>> = const { RefCell::new(None) };
    /// SDL without a window, for headless runs.
    static HEADLESS_SDL: RefCell<Option<Sdl>> = const { RefCell::new(None) };
}

/// Milliseconds are counted from here (SDL_GetTicks counts from SDL_Init).
static START: std::sync::OnceLock<std::time::Instant> = std::sync::OnceLock::new();

/// A quit request stays set: menus nested several deep must all see it.
static QUIT_REQUESTED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

fn with<R>(f: impl FnOnce(&mut Platform) -> R) -> Option<R> {
    PLATFORM.with(|p| p.borrow_mut().as_mut().map(f))
}

pub fn sys_set_screenshot(f: fn(&Path)) {
    SCREENSHOT_FN.with(|s| *s.borrow_mut() = Some(f));
}

/// The SDL context (for the sound and timer), with or without a window.
pub fn sdl() -> Option<Sdl> {
    with(|p| p.sdl.clone()).or_else(|| HEADLESS_SDL.with(|h| h.borrow().clone()))
}

pub fn sys_init(title: &str, opt: &SysOptions) -> Result<(), String> {
    START.get_or_init(std::time::Instant::now);
    let sdl = sdl2::init()?;
    if opt.hidden {
        HEADLESS_SDL.with(|h| *h.borrow_mut() = Some(sdl));
        return Ok(());
    }
    let video = sdl.video()?;
    let attr = video.gl_attr();
    // In the browser: WebGL 1 (OpenGL ES 2), on which Emscripten emulates the fixed
    // function pipeline.
    if cfg!(target_os = "emscripten") {
        attr.set_context_profile(GLProfile::GLES);
        attr.set_context_version(2, 0);
    } else {
        attr.set_context_profile(GLProfile::Compatibility);
        attr.set_context_version(2, 1);
    }
    attr.set_depth_size(24);
    attr.set_double_buffer(true);
    let scale = opt.scale.max(1);
    let mut builder = video.window(title, SCREEN_W as u32 * scale, SCREEN_H as u32 * scale);
    builder.position_centered().opengl();
    // In the browser the page scales the canvas; a resizable window would follow the
    // canvas' size on the page and draw the 640x480 pixels at uneven widths.
    if !cfg!(target_os = "emscripten") {
        builder.resizable();
    }
    let window = builder.build().map_err(|e| e.to_string())?;
    let gl_context = window.gl_create_context()?;
    gl_load(|name| video.gl_get_proc_address(name) as *const c_void)?;
    let _ = video.gl_set_swap_interval(1);
    let mut screen_texture = 0;
    // SAFETY: the context is current; the texture is allocated with no data.
    unsafe {
        GenTextures(1, &mut screen_texture);
        BindTexture(GL_TEXTURE_2D, screen_texture);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // 640x480 isn't a power of two: WebGL 1 only samples such textures when clamped.
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA as GLint, SCREEN_W as GLsizei, SCREEN_H as GLsizei, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, std::ptr::null());
    }
    video.text_input().stop(); // only while an edit box is open
    let lines = opt.script.as_ref().map(|path| load_script(path)).unwrap_or_default();
    let events = sdl.event_pump()?;
    let event_subsystem = sdl.event()?;
    let joystick_subsystem = sdl.joystick().ok();
    PLATFORM.with(|p| {
        *p.borrow_mut() = Some(Platform {
            sdl,
            video,
            window,
            _gl_context: gl_context,
            events,
            event_subsystem,
            joystick_subsystem,
            joysticks: [None, None],
            screen_texture,
            rgba: vec![0; SCREEN_W * SCREEN_H * 4],
            mouse: (0, 0, false),
            script: Script {
                lines,
                pos: 0,
                resume: 0,
                fake_alt: false,
                fake_key: None,
                fake_key_until: 0,
                held: Vec::new(),
                glshot: None,
            },
        })
    });
    Ok(())
}

pub fn sys_shutdown() {
    with(|p| unsafe { DeleteTextures(1, &p.screen_texture) });
    PLATFORM.with(|p| *p.borrow_mut() = None);
    HEADLESS_SDL.with(|h| *h.borrow_mut() = None);
}

fn load_script(path: &Path) -> Vec<String> {
    match std::fs::read_to_string(path) {
        Ok(text) => text
            .lines()
            .map(|l| l.trim_end_matches('\r').to_string())
            .filter(|l| !l.is_empty() && !l.starts_with('#'))
            .collect(),
        Err(_) => {
            eprintln!("script: cannot open {}", path.display());
            Vec::new()
        }
    }
}

fn key_by_name(n: &str) -> Option<Keycode> {
    Some(match n {
        "enter" => Keycode::Return,
        "escape" => Keycode::Escape,
        "up" => Keycode::Up,
        "down" => Keycode::Down,
        "left" => Keycode::Left,
        "right" => Keycode::Right,
        "tab" => Keycode::Tab,
        "space" => Keycode::Space,
        "backspace" => Keycode::Backspace,
        _ if n.len() >= 2 && n.starts_with('f') && n[1..].parse::<i32>().is_ok() => {
            Keycode::from_i32(Keycode::F1.into_i32() + n[1..].parse::<i32>().ok()? - 1)?
        }
        _ => Keycode::from_i32(*n.as_bytes().first()? as i32)?,
    })
}

impl Platform {
    fn ticks(&self) -> u32 {
        sys_ticks()
    }

    fn push_key(&mut self, k: Keycode) {
        let sc = Scancode::from_keycode(k);
        self.script.fake_key = sc;
        self.script.fake_key_until = self.ticks() + 100;
        {
            let ev = &self.event_subsystem;
            let mk = |down: bool| {
                let (timestamp, window_id, keymod, repeat) = (0, 0, Mod::NOMOD, false);
                if down {
                    Event::KeyDown { timestamp, window_id, keycode: Some(k), scancode: sc, keymod, repeat }
                } else {
                    Event::KeyUp { timestamp, window_id, keycode: Some(k), scancode: sc, keymod, repeat }
                }
            };
            let _ = ev.push_event(mk(true));
            let _ = ev.push_event(mk(false));
        }
    }

    /// Runs the due script commands; returns the front-screen shots to take.
    fn run_script(&mut self) -> Vec<PathBuf> {
        let mut shots = Vec::new();
        while self.script.pos < self.script.lines.len() && self.ticks() >= self.script.resume {
            let cmd = self.script.lines[self.script.pos].clone();
            self.script.pos += 1;
            self.script.fake_alt = false;
            let (word, arg) = cmd.split_once(' ').unwrap_or((cmd.as_str(), ""));
            let ev = Some(&self.event_subsystem);
            match word {
                "wait" => self.script.resume = self.ticks() + arg.trim().parse::<u32>().unwrap_or(0),
                "hold" => {
                    let mut it = arg.split_whitespace();
                    let name = it.next().unwrap_or("");
                    let ms: u32 = it.next().and_then(|m| m.parse().ok()).unwrap_or(0);
                    if let Some(sc) = key_by_name(name).and_then(Scancode::from_keycode) {
                        let until = self.ticks() + ms;
                        self.script.held.retain(|h| h.1 > until - ms);
                        if self.script.held.len() < 4 {
                            self.script.held.push((sc, until));
                        }
                    }
                }
                "key" => {
                    self.script.fake_alt = arg.starts_with("alt+");
                    let name = arg.strip_prefix("alt+").unwrap_or(arg);
                    if let Some(k) = key_by_name(name) {
                        self.push_key(k);
                    }
                    self.script.resume = self.ticks() + 50;
                    break;
                }
                "text" => {
                    if let Some(ev) = ev {
                        let _ = ev.push_event(Event::TextInput { timestamp: 0, window_id: 0, text: arg.to_string() });
                    }
                }
                "click" => {
                    let mut it = arg.split_whitespace().map(|v| v.parse::<i32>().unwrap_or(0));
                    let (x, y) = (it.next().unwrap_or(0), it.next().unwrap_or(0));
                    if let Some(ev) = ev {
                        let (timestamp, window_id, which, mouse_btn, clicks) = (0, 0, 0, MouseButton::Left, 1);
                        let _ = ev.push_event(Event::MouseButtonDown { timestamp, window_id, which, mouse_btn, clicks, x, y });
                        let _ = ev.push_event(Event::MouseButtonUp { timestamp, window_id, which, mouse_btn, clicks, x, y });
                    }
                    self.script.resume = self.ticks() + 50;
                    break;
                }
                "shot" => shots.push(PathBuf::from(arg)),
                // Taken from the next frame, before it is shown.
                "glshot" => self.script.glshot = Some(PathBuf::from(arg)),
                "quit" => {
                    if let Some(ev) = ev {
                        let _ = ev.push_event(Event::Quit { timestamp: 0 });
                    }
                }
                _ => {}
            }
        }
        shots
    }

    /// The 4:3 picture's place in a w x h area (letterboxed).
    fn fit_4_3(w: i32, h: i32) -> (i32, i32, i32, i32) {
        let (rw, rh) = if w * SCREEN_H as i32 > h * SCREEN_W as i32 {
            (h * SCREEN_W as i32 / SCREEN_H as i32, h)
        } else {
            (w, w * SCREEN_H as i32 / SCREEN_W as i32)
        };
        ((w - rw) / 2, (h - rh) / 2, rw, rh)
    }

    fn picture_rect(&self) -> (i32, i32, i32, i32) {
        let (w, h) = self.window.drawable_size();
        Self::fit_4_3(w as i32, h as i32)
    }

    /// Window coordinates to 640x480.
    fn map_mouse(&self, x: i32, y: i32) -> (i32, i32) {
        let (w, h) = self.window.size();
        let (rx, ry, rw, rh) = Self::fit_4_3(w as i32, h as i32);
        (
            if rw != 0 { (x - rx) * SCREEN_W as i32 / rw } else { 0 },
            if rh != 0 { (y - ry) * SCREEN_H as i32 / rh } else { 0 },
        )
    }

    /// The indexed screen as RGBA; palette index `transparent` (if any) clear.
    fn screen_to_rgba(&mut self, pixels: &[u8], palette: &[u8], transparent: Option<u8>) {
        let mut lut = [[0u8; 4]; 256];
        for (i, c) in lut.iter_mut().enumerate() {
            let a = if Some(i as u8) == transparent { 0 } else { 0xff };
            *c = [palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2], a];
        }
        for (out, &p) in self.rgba.chunks_exact_mut(4).zip(pixels) {
            out.copy_from_slice(&lut[p as usize]);
        }
    }

    /// Draws the screen texture over the whole picture area.
    fn draw_screen(&self, blend: bool) {
        let (x, y, w, h) = self.picture_rect();
        // SAFETY: plain GL state calls; rgba holds a 640x480 RGBA picture.
        unsafe {
            Viewport(x, y, w, h);
            Disable(GL_DEPTH_TEST);
            Disable(GL_CULL_FACE);
            Disable(GL_FOG);
            MatrixMode(GL_PROJECTION);
            LoadIdentity();
            Ortho(0.0, SCREEN_W as f64, SCREEN_H as f64, 0.0, -1.0, 1.0);
            MatrixMode(GL_MODELVIEW);
            LoadIdentity();
            Enable(GL_TEXTURE_2D);
            BindTexture(GL_TEXTURE_2D, self.screen_texture);
            TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            PixelStorei(GL_UNPACK_ALIGNMENT, 4);
            TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W as GLsizei, SCREEN_H as GLsizei, GL_RGBA, GL_UNSIGNED_BYTE,
                self.rgba.as_ptr() as *const c_void);
            if blend {
                Enable(GL_ALPHA_TEST);
                AlphaFunc(GL_GREATER, 0.5);
            }
            Begin(GL_QUADS);
            TexCoord2f(0.0, 0.0);
            Vertex2f(0.0, 0.0);
            TexCoord2f(1.0, 0.0);
            Vertex2f(SCREEN_W as f32, 0.0);
            TexCoord2f(1.0, 1.0);
            Vertex2f(SCREEN_W as f32, SCREEN_H as f32);
            TexCoord2f(0.0, 1.0);
            Vertex2f(0.0, SCREEN_H as f32);
            End();
            Disable(GL_ALPHA_TEST);
            Disable(GL_TEXTURE_2D);
        }
    }

    fn read_picture(&self) -> (Vec<u8>, usize, usize) {
        let (x, y, w, h) = self.picture_rect();
        let (w, h) = (w.max(0) as usize, h.max(0) as usize);
        let mut rgb = vec![0u8; w * h * 3];
        // SAFETY: rgb holds w * h RGB pixels with byte alignment.
        unsafe {
            PixelStorei(GL_PACK_ALIGNMENT, 1);
            ReadPixels(x, y, w as GLsizei, h as GLsizei, GL_RGB, GL_UNSIGNED_BYTE, rgb.as_mut_ptr() as *mut c_void);
        }
        // Bottom-up to top-down.
        let row = w * 3;
        let flipped: Vec<u8> = rgb.chunks(row.max(1)).rev().flatten().copied().collect();
        (flipped, w, h)
    }

    fn swap(&mut self) {
        if let Some(path) = self.script.glshot.take() {
            let (rgb, w, h) = self.read_picture();
            let _ = png_write_rgb(&path, &rgb, w, h);
        }
        self.window.gl_swap_window();
    }
}

/// Shows an indexed 640x480 picture with the given palette.
pub fn sys_present(pixels: &[u8], palette: &[u8]) {
    with(|p| {
        let (w, h) = p.window.drawable_size();
        // SAFETY: plain GL state calls.
        unsafe {
            Viewport(0, 0, w as GLsizei, h as GLsizei);
            ClearColor(0.0, 0.0, 0.0, 1.0);
            Clear(GL_COLOR_BUFFER_BIT);
        }
        p.screen_to_rgba(pixels, palette, None);
        p.draw_screen(false);
        p.swap();
    });
    yield_frame();
}

/// OpenGL drawing (the race) is available.
pub fn sys_gl() -> bool {
    with(|_| ()).is_some()
}

pub fn sys_gl_begin_frame() {
    with(|p| {
        let (w, h) = p.window.drawable_size();
        // SAFETY: plain GL state calls.
        unsafe {
            Viewport(0, 0, w as GLsizei, h as GLsizei);
            Disable(GL_SCISSOR_TEST);
            ClearColor(0.0, 0.0, 0.0, 1.0);
            DepthMask(GL_TRUE);
            Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    });
}

/// Restricts drawing to a rectangle of the 640x480 frame.
pub fn sys_gl_viewport(x: i32, y: i32, w: i32, h: i32) {
    with(|p| {
        let (rx, ry, rw, rh) = p.picture_rect();
        let (sw, sh) = (SCREEN_W as i32, SCREEN_H as i32);
        let gx = rx + x * rw / sw;
        let gw = w * rw / sw;
        let gh = h * rh / sh;
        let gy = ry + rh - (y * rh / sh) - gh;
        // SAFETY: plain GL state calls.
        unsafe {
            Viewport(gx, gy, gw, gh);
            Scissor(gx, gy, gw, gh);
            Enable(GL_SCISSOR_TEST);
        }
    });
}

/// Draws an indexed 640x480 picture over the frame; index `transparent` stays see-through.
pub fn sys_gl_overlay(pixels: &[u8], palette: &[u8], transparent: u8) {
    with(|p| {
        // SAFETY: plain GL state call.
        unsafe { Disable(GL_SCISSOR_TEST) };
        p.screen_to_rgba(pixels, palette, Some(transparent));
        p.draw_screen(true);
    });
}

pub fn sys_gl_swap() {
    with(|p| p.swap());
    yield_frame();
}

fn map_key(k: Keycode) -> i32 {
    match k {
        Keycode::Escape => KEY_ESCAPE,
        Keycode::Return | Keycode::KpEnter => KEY_ENTER,
        Keycode::Up => KEY_UP,
        Keycode::Down => KEY_DOWN,
        Keycode::Left => KEY_LEFT,
        Keycode::Right => KEY_RIGHT,
        _ => {
            let v = k.into_i32();
            if v > 0 && v < 128 { v } else { KEY_NONE }
        }
    }
}

/// Gathers input since the last call.
pub fn sys_poll(input: &mut SysInput) {
    input.mouse_clicked = false;
    input.mouse_pressed = false;
    input.key = KEY_NONE;
    input.text.clear();
    input.backspaces = 0;
    let shots = with(|p| {
        let shots = p.run_script();
        let events: Vec<Event> = p.events.poll_iter().collect();
        for e in events {
            match e {
                Event::Quit { .. } => QUIT_REQUESTED.store(true, std::sync::atomic::Ordering::Relaxed),
                Event::MouseMotion { x, y, .. } => {
                    let (mx, my) = p.map_mouse(x, y);
                    p.mouse.0 = mx;
                    p.mouse.1 = my;
                }
                Event::MouseButtonDown { mouse_btn: MouseButton::Left, x, y, .. } => {
                    let (mx, my) = p.map_mouse(x, y);
                    p.mouse = (mx, my, true);
                    input.mouse_pressed = true;
                }
                Event::MouseButtonUp { mouse_btn: MouseButton::Left, x, y, .. } => {
                    let (mx, my) = p.map_mouse(x, y);
                    p.mouse = (mx, my, false);
                    input.mouse_clicked = true;
                }
                Event::KeyDown { keycode: Some(k), .. } => {
                    input.key = map_key(k);
                    if k == Keycode::Backspace {
                        input.backspaces += 1;
                    }
                }
                Event::TextInput { text, .. } => {
                    // To Windows-1252 (the Latin-1 range), as the game's text is.
                    for c in text.chars() {
                        if (c as u32) < 0x100 && input.text.len() < 31 {
                            input.text.push(c as u32 as u8);
                        }
                    }
                }
                _ => {}
            }
        }
        input.mouse_x = p.mouse.0;
        input.mouse_y = p.mouse.1;
        input.mouse_down = p.mouse.2;
        shots
    })
    .unwrap_or_default();
    for path in shots {
        if let Some(f) = SCREENSHOT_FN.with(|s| *s.borrow()) {
            f(&path);
        }
    }
    input.quit = sys_quit_requested();
}

/// The window was closed (or the process asked to stop); stays true from then on.
pub fn sys_quit_requested() -> bool {
    QUIT_REQUESTED.load(std::sync::atomic::Ordering::Relaxed)
}

pub fn sys_ticks() -> u32 {
    START.get_or_init(std::time::Instant::now).elapsed().as_millis() as u32
}

#[cfg(not(target_os = "emscripten"))]
pub fn sys_sleep(ms: u32) {
    std::thread::sleep(std::time::Duration::from_millis(ms as u64));
}

#[cfg(target_os = "emscripten")]
extern "C" {
    fn emscripten_sleep(ms: u32);
}

/// In the browser the game's loops run on the page's only thread: sleeping (Asyncify)
/// hands control back to the browser, which then shows the frame and delivers input.
#[cfg(target_os = "emscripten")]
pub fn sys_sleep(ms: u32) {
    // SAFETY: built with -sASYNCIFY, so the call may suspend and resume the program.
    unsafe { emscripten_sleep(ms) };
}

/// After each frame: in the browser the picture only appears once the browser gets
/// control back, also in loops that don't sleep (the race).
fn yield_frame() {
    #[cfg(target_os = "emscripten")]
    sys_sleep(0);
}

/// Windows virtual-key codes to SDL scancodes, for the keys the game asks about.
fn vk_to_scancode(vk: i32) -> Option<Scancode> {
    let off = |base: Scancode, n: i32| Scancode::from_i32(base as i32 + n);
    match vk {
        0x41..=0x5a => off(Scancode::A, vk - 0x41),
        0x31..=0x39 => off(Scancode::Num1, vk - 0x31),
        0x70..=0x7b => off(Scancode::F1, vk - 0x70),
        0x61..=0x69 => off(Scancode::Kp1, vk - 0x61),
        0x30 => Some(Scancode::Num0),
        0x60 => Some(Scancode::Kp0),
        0x08 => Some(Scancode::Backspace),
        0x09 => Some(Scancode::Tab),
        0x0d => Some(Scancode::Return),
        0x10 => Some(Scancode::LShift),
        0x11 => Some(Scancode::LCtrl),
        0x12 => Some(Scancode::LAlt),
        0x1b => Some(Scancode::Escape),
        0x20 => Some(Scancode::Space),
        0x25 => Some(Scancode::Left),
        0x26 => Some(Scancode::Up),
        0x27 => Some(Scancode::Right),
        0x28 => Some(Scancode::Down),
        _ => None,
    }
}

/// Whether a key (Windows virtual-key code, as the game stores them) is held now.
pub fn keyispressed(vk: i32) -> bool {
    with(|p| {
        if vk == 0x12 && p.script.fake_alt {
            return true;
        }
        p.events.pump_events();
        let Some(sc) = vk_to_scancode(vk) else { return false };
        let now = p.ticks();
        if p.script.fake_key == Some(sc) && now < p.script.fake_key_until {
            return true;
        }
        if p.script.held.iter().any(|&(k, until)| k == sc && now < until) {
            return true;
        }
        let state = p.events.keyboard_state();
        match vk {
            0x10 => state.is_scancode_pressed(Scancode::LShift) || state.is_scancode_pressed(Scancode::RShift),
            0x11 => state.is_scancode_pressed(Scancode::LCtrl) || state.is_scancode_pressed(Scancode::RCtrl),
            0x12 => state.is_scancode_pressed(Scancode::LAlt) || state.is_scancode_pressed(Scancode::RAlt),
            0x0d => state.is_scancode_pressed(Scancode::Return) || state.is_scancode_pressed(Scancode::KpEnter),
            _ => state.is_scancode_pressed(sc),
        }
    })
    .unwrap_or(false)
}

/// Joystick n (0 or 1) as winmm's joyGetPosEx reports it: x and y 0..65535 (centre
/// 32767) and a bit per button; None when there is no such joystick.
pub fn sys_joystick_position(n: usize) -> Option<(u32, u32, u32)> {
    with(|p| {
        if n > 1 {
            return None;
        }
        let sub = p.joystick_subsystem.as_ref()?;
        sub.update();
        if p.joysticks[n].as_ref().is_some_and(|j| !j.attached()) {
            p.joysticks[n] = None;
        }
        if p.joysticks[n].is_none() {
            if sub.num_joysticks().ok()? as usize <= n {
                return None;
            }
            p.joysticks[n] = sub.open(n as u32).ok();
        }
        let j = p.joysticks[n].as_ref()?;
        let axis = |i| j.axis(i).map(|v| (v as i32 + 32768) as u32).unwrap_or(32767);
        let buttons = (0..j.num_buttons().min(32))
            .filter(|&i| j.button(i).unwrap_or(false))
            .fold(0u32, |b, i| b | 1 << i);
        Some((axis(0), axis(1), buttons))
    })
    .flatten()
}

/// Typed characters arrive in SysInput::text while text input is on.
pub fn sys_text_input(on: bool) {
    with(|p| {
        if on {
            p.video.text_input().start();
        } else {
            p.video.text_input().stop();
        }
    });
}

/// The mouse state right now (unused by the game; kept for debugging).
#[allow(dead_code)]
pub fn sys_mouse_state() -> Option<MouseState> {
    with(|p| p.events.mouse_state())
}
