//! Helpers shared by the menus.

use crate::game::Game;
use crate::gfx::screen::with_screen;
use crate::res::pcx::pcx_decode;

/// load_pcx_file_1 flags (rcpcx.cpp): the picture, read the palette, apply it.
pub const PCX_IMAGE: i32 = 1;
pub const PCX_PALETTE: i32 = 2;
pub const PCX_SET_PALETTE: i32 = 4;

/// Loads a menu picture from A2MRCS into a screen; 0, or 1000 if it is missing.
pub fn load_pcx_file_1(g: &Game, which: usize, name: &str, flags: i32) -> i32 {
    let Some(img) = g.menu_files.find(name).and_then(pcx_decode) else {
        eprintln!("load_pcx_file: cannot load {name}");
        return 1000;
    };
    with_screen(|s| {
        if flags & PCX_IMAGE != 0 {
            s.blit(which, &img.pixels, img.width, img.height, 0, 0, None);
        }
        if flags & (PCX_PALETTE | PCX_SET_PALETTE) == PCX_PALETTE | PCX_SET_PALETTE {
            s.palette = img.palette;
        }
    });
    0
}

/// printf's "%-Ns": the text padded with blanks to N characters.
pub fn pad_right(text: &[u8], n: usize) -> Vec<u8> {
    let mut out = text.to_vec();
    while out.len() < n {
        out.push(b' ');
    }
    out
}

/// printf's "%Nd".
pub fn num_right(v: i32, n: usize) -> Vec<u8> {
    format!("{v:>n$}").into_bytes()
}

/// printf's "%-Nd".
pub fn num_left(v: i32, n: usize) -> Vec<u8> {
    format!("{v:<n$}").into_bytes()
}

/// "HHuMMmSSs".
pub fn time_string(t: i32) -> Vec<u8> {
    format!("{:02}u{:02}m{:02}s", t / 3600, t / 60 % 60, t % 60).into_bytes()
}
