//! Text drawing (font.cpp). The original created Windows GDI fonts and drew with
//! ExtTextOut into the DirectDraw surfaces; the port renders the same TrueType font
//! (Arial Black) itself, unsmoothed, in the nearest palette colour.

use std::cell::RefCell;
use std::path::PathBuf;

use fontdue::{Font, FontSettings};

use crate::gfx::screen::with_screen;
use crate::platform::sys::{SCREEN_H, SCREEN_W};

pub const MAX_FONTS: usize = 6;

struct XvgaFont {
    font: Font,
    /// Pixels per em.
    px: f32,
    /// COLORREF values: 0x00BBGGRR.
    background: u32,
    text: u32,
    /// 1 transparent, 2 opaque (GDI's TRANSPARENT/OPAQUE).
    bkmode: i32,
}

#[derive(Default)]
struct Fonts {
    fonts: [Option<XvgaFont>; MAX_FONTS],
    active_font: i32,
    extra_dir: Option<PathBuf>,
}

thread_local! {
    static FONTS: RefCell<Fonts> = RefCell::new(Fonts { active_font: -1, ..Default::default() });
}

/// Where to look for font files besides the system font folders.
pub fn font_init(font_dir: Option<PathBuf>) {
    FONTS.with(|f| f.borrow_mut().extra_dir = font_dir);
}

pub fn font_shutdown() {
    FONTS.with(|f| f.borrow_mut().fonts = Default::default());
}

/// Font files for a face name: the Windows file names, and those used by the
/// Debian/Ubuntu msttcorefonts package and macOS.
fn files_for(face: &str) -> &'static [&'static str] {
    if face == "Arial Black" {
        &["ariblk.ttf", "ARIBLK.TTF", "Arial_Black.ttf", "Arial Black.ttf"]
    } else {
        &["arial.ttf", "ARIAL.TTF", "Arial.ttf"]
    }
}

fn open_face(extra_dir: &Option<PathBuf>, face: &str) -> Option<Font> {
    let mut dirs: Vec<PathBuf> = Vec::new();
    dirs.extend(extra_dir.clone());
    if let Ok(w) = std::env::var("WINDIR") {
        dirs.push(PathBuf::from(w).join("Fonts"));
    }
    for d in [
        "/mnt/c/Windows/Fonts", // Linux under WSL
        "/usr/share/fonts/truetype/msttcorefonts",
        "/System/Library/Fonts/Supplemental",
        "/Library/Fonts",
    ] {
        dirs.push(PathBuf::from(d));
    }
    for d in &dirs {
        for f in files_for(face) {
            if let Ok(data) = std::fs::read(d.join(f)) {
                if let Ok(font) = Font::from_bytes(data, FontSettings::default()) {
                    return Some(font);
                }
            }
        }
    }
    None
}

/// The cell height (ascent + descent) at a size, in whole pixels.
fn cell_height(font: &Font, px: f32) -> i32 {
    font.horizontal_line_metrics(px)
        .map(|m| m.ascent.ceil() as i32 - m.descent.floor() as i32)
        .unwrap_or(px as i32)
}

/// height: GDI cell height in pixels (CreateFont's positive nHeight). Returns 0, or
/// 10000 when the font cannot be created (as the original).
pub fn xvga_load_font(index: usize, face: &str, height: i32, _italic: bool, _underline: bool) -> i32 {
    FONTS.with(|f| {
        let mut f = f.borrow_mut();
        if index >= MAX_FONTS || f.fonts[index].is_some() {
            return 10000;
        }
        let Some(font) = open_face(&f.extra_dir, face) else {
            eprintln!("font: cannot find \"{face}\" (pass --fonts <dir> with ariblk.ttf)");
            return 10000;
        };
        // GDI's positive height is the cell height; find the size that gives it.
        let Some(px) = (2..=height).rev().map(|s| s as f32).find(|&px| cell_height(&font, px) <= height) else {
            return 10000;
        };
        f.fonts[index] = Some(XvgaFont { font, px, background: 0, text: 0xffffff, bkmode: 2 });
        0
    })
}

pub fn xvga_select_font(index: i32) {
    if (0..MAX_FONTS as i32).contains(&index) {
        FONTS.with(|f| f.borrow_mut().active_font = index);
    }
}

pub fn xvga_set_font_colors(index: usize, background: u32, text: u32) {
    FONTS.with(|f| {
        if let Some(font) = f.borrow_mut().fonts.get_mut(index).and_then(|x| x.as_mut()) {
            font.background = background;
            font.text = text;
        }
    });
}

pub fn xvga_set_font_bkmode(index: usize, mode: i32) {
    FONTS.with(|f| {
        if let Some(font) = f.borrow_mut().fonts.get_mut(index).and_then(|x| x.as_mut()) {
            font.bkmode = mode;
        }
    });
}

pub fn xvga_get_active_font() -> i32 {
    FONTS.with(|f| f.borrow().active_font)
}

pub fn xvga_free_font(index: usize) {
    FONTS.with(|f| {
        if let Some(slot) = f.borrow_mut().fonts.get_mut(index) {
            *slot = None;
        }
    });
}

fn with_active<R>(f: impl FnOnce(&XvgaFont) -> R) -> Option<R> {
    FONTS.with(|fonts| {
        let fonts = fonts.borrow();
        let i = fonts.active_font;
        if i < 0 {
            return None;
        }
        fonts.fonts[i as usize].as_ref().map(f)
    })
}

/// Windows-1252 bytes as characters (the game's text is Latin-1 in practice).
fn chars(text: &[u8]) -> impl Iterator<Item = char> + '_ {
    text.iter().map(|&c| c as char)
}

/// Each character's pen position along the line, and the total width.
fn layout(font: &XvgaFont, text: &[u8]) -> (Vec<(char, f32)>, f32) {
    let mut pen = 0.0f32;
    let mut out = Vec::new();
    let mut prev: Option<char> = None;
    for c in chars(text) {
        if let Some(p) = prev {
            pen += font.font.horizontal_kern(p, c, font.px).unwrap_or(0.0);
        }
        out.push((c, pen));
        pen += font.font.metrics(c, font.px).advance_width;
        prev = Some(c);
    }
    (out, pen)
}

pub fn get_string_width(text: &[u8], length: usize) -> i32 {
    let text = &text[..length.min(text.len())];
    with_active(|f| layout(f, text).1.round() as i32).unwrap_or(0)
}

pub fn get_string_height(_text: &[u8], _length: usize) -> i32 {
    with_active(|f| cell_height(&f.font, f.px)).unwrap_or(0)
}

/// Prints text with the selected font into screen `which` (0 front, 1 back, 2
/// buffer), the top of the text cell at y.
pub fn win_print_xy(which: usize, x: i32, y: i32, text: &[u8]) -> i32 {
    if text.is_empty() {
        return 0;
    }
    let drawn = with_active(|f| {
        let ascent = f.font.horizontal_line_metrics(f.px).map(|m| m.ascent.ceil() as i32).unwrap_or(0);
        let (glyphs, width) = layout(f, text);
        let (w, h) = (width.round() as i32, cell_height(&f.font, f.px));
        // The text's cell: background where no glyph covers it (in opaque mode).
        let mut mask = vec![false; (w.max(0) * h.max(0)) as usize];
        for (c, pen) in glyphs {
            let (m, bitmap) = f.font.rasterize(c, f.px);
            let gx = pen.round() as i32 + m.xmin;
            let gy = ascent - m.height as i32 - m.ymin;
            for row in 0..m.height as i32 {
                for col in 0..m.width as i32 {
                    // Unsmoothed: a pixel is on when the glyph covers at least half of it.
                    if bitmap[(row * m.width as i32 + col) as usize] >= 128 {
                        let (px, py) = (gx + col, gy + row);
                        if px >= 0 && px < w && py >= 0 && py < h {
                            mask[(py * w + px) as usize] = true;
                        }
                    }
                }
            }
        }
        (mask, w, h, f.text, f.background, f.bkmode)
    });
    let Some((mask, w, h, text_color, bg_color, bkmode)) = drawn else { return 0 };
    with_screen(|s| {
        let (fg, bg) = (s.nearest_color(text_color), s.nearest_color(bg_color));
        for row in 0..h {
            let sy = y + row;
            if sy < 0 || sy >= SCREEN_H as i32 {
                continue;
            }
            for col in 0..w {
                let sx = x + col;
                if sx < 0 || sx >= SCREEN_W as i32 {
                    continue;
                }
                let at = sy as usize * SCREEN_W + sx as usize;
                if mask[(row * w + col) as usize] {
                    s.pixels[which][at] = fg;
                } else if bkmode == 2 {
                    s.pixels[which][at] = bg;
                }
            }
        }
    });
    0
}
