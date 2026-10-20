//! Screen transitions (wipes.cpp and its wipe*.cpp files): bring the picture in a back
//! or buffer screen onto the front screen.

use crate::gfx::screen::{blt_scr_2_scr, with_screen, xvga_show, SCREEN_FRONT};
use crate::platform::sys::{sys_sleep, SCREEN_H, SCREEN_W};

pub const WIPE_VROLLBAR_UP: i32 = 1;
pub const WIPE_VROLLBAR_DOWN: i32 = 2;
pub const WIPE_PIXELFADE: i32 = 3;
pub const WIPE_SCROLL_LEFT: i32 = 4;
pub const WIPE_SCROLL_TOP: i32 = 5;
pub const WIPE_SCROLL_RIGHT: i32 = 6;
pub const WIPE_SCROLL_BOTTOM: i32 = 7;
pub const WIPE_LUXAFLEX_HORIZONTAL: i32 = 8;
pub const WIPE_LUXAFLEX_VERTICAL: i32 = 9;

/// The original drew each wipe step straight onto the visible surface and only called
/// Sleep(0) between steps, so the speed depended on the machine. The port shows every
/// step and waits this long, about what a 1997 PC took for a full-screen pass.
const WIPE_STEP_MS: u32 = 25;

/// raster: four 4x4 masks; pass s copies the pixels whose mask byte is set. Laid out as
/// in A2MENU.DAT: byte [s][row + col * 4].
const RASTER: [[u8; 16]; 4] = [
    [1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1],
    [0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0],
    [0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0],
    [0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0],
];

fn step_done() {
    xvga_show();
    sys_sleep(WIPE_STEP_MS);
}

fn wipe_draw_pixel_grid(src: usize, step: usize, x: i32, y: i32, w: i32, h: i32) {
    with_screen(|s| {
        for by in (y..y + h).step_by(4) {
            for bx in (x..x + w).step_by(4) {
                for row in 0..4 {
                    if by + row >= SCREEN_H as i32 {
                        break;
                    }
                    for col in 0..4 {
                        if RASTER[step][(row + col * 4) as usize] != 0 && bx + col < SCREEN_W as i32 {
                            let o = (by + row) as usize * SCREEN_W + (bx + col) as usize;
                            s.pixels[SCREEN_FRONT][o] = s.pixels[src][o];
                        }
                    }
                }
            }
        }
    });
}

fn wipe_pixelfade_full(src: usize, x: i32, y: i32, w: i32, h: i32) {
    for i in 0..4 {
        wipe_draw_pixel_grid(src, i, x, y, w, h);
        step_done();
    }
}

/// Scrolls the picture in from the left edge, 8 pixels at a time.
fn wipe_scroll_in_left(src: usize, x: i32, y: i32, w: i32, h: i32) {
    let mut edge = x + w - 8;
    while edge >= 0 {
        let width = x + w - edge;
        with_screen(|s| {
            for row in y..y + h {
                for i in 0..width.min(SCREEN_W as i32 - x) {
                    let r = row as usize * SCREEN_W;
                    s.pixels[SCREEN_FRONT][r + (x + i) as usize] = s.pixels[src][r + (edge + i) as usize];
                }
            }
        });
        step_done();
        edge -= 8;
    }
}

pub fn handle_wipe_1(src: usize, ty: i32, x: i32, y: i32, w: i32, h: i32, _delay: i32) {
    match ty {
        WIPE_PIXELFADE => wipe_pixelfade_full(src, x, y, w, h),
        WIPE_SCROLL_LEFT => wipe_scroll_in_left(src, x, y, w, h),
        _ => {
            // Only the two above are used by the game; the others copy straight away.
            eprintln!("handle_wipe: type {ty} not ported, copying");
            blt_scr_2_scr(src, SCREEN_FRONT, x, y, x + w, y + h, 0);
            xvga_show();
        }
    }
}

pub fn handle_wipe(src: usize, ty: i32, delay: i32) {
    handle_wipe_1(src, ty, 0, 0, SCREEN_W as i32, SCREEN_H as i32, delay);
}
