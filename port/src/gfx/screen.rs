//! The 640x480 8-bit screen, the port's stand-in for the xvga library's DirectDraw
//! surfaces (initlib.cpp): a front screen shown to the player and back/buffer screens
//! drawn into off-screen, all sharing one 256-colour palette. Also the xvga library's
//! blits (clipblit.cpp) under their original names.

use std::cell::RefCell;
use std::rc::Rc;

use crate::platform::sys::{sys_present, SCREEN_H, SCREEN_W};
use crate::res::archive::{Sprite, SpriteArchive};

pub const SCREEN_FRONT: usize = 0;
pub const SCREEN_BACK: usize = 1;
pub const SCREEN_BUFFER: usize = 2;
pub const SCREEN_COUNT: usize = 3;

/// Palette index that set_mem_image leaves out (blt_sprite skips 0xFF).
pub const MEM_IMAGE_TRANSPARENT: u8 = 0xff;
/// set_mem_image flags: clip to the clip limits.
pub const MI_CLIP: i32 = 1;
/// set_mem_image flags: x/y is the top-left corner instead of the image's centre point.
pub const MI_TOPLEFT: i32 = 2;

pub struct Screen {
    pub pixels: [Vec<u8>; SCREEN_COUNT],
    pub palette: [u8; 768],
    /// The sprites used as mem images, numbered from 1 (the menu's A2MRC, the race's GAMESPR).
    pub mem_images: Option<Rc<SpriteArchive>>,
    clip: (i32, i32, i32, i32),
}

thread_local! {
    static SCREEN: RefCell<Screen> = RefCell::new(Screen {
        pixels: [vec![0; SCREEN_W * SCREEN_H], vec![0; SCREEN_W * SCREEN_H], vec![0; SCREEN_W * SCREEN_H]],
        palette: [0; 768],
        mem_images: None,
        clip: (0, 0, SCREEN_W as i32 - 1, SCREEN_H as i32 - 1),
    });
}

/// The screens, for drawing straight into them.
pub fn with_screen<R>(f: impl FnOnce(&mut Screen) -> R) -> R {
    SCREEN.with(|s| f(&mut s.borrow_mut()))
}

pub fn screen_clear(which: usize, color: u8) {
    with_screen(|s| s.pixels[which].fill(color));
}

pub fn screen_copy(dst: usize, src: usize) {
    with_screen(|s| {
        let p = s.pixels[src].clone();
        s.pixels[dst].copy_from_slice(&p);
    });
}

impl Screen {
    /// Draws an image with its top-left corner at x, y, clipped to the screen; with
    /// `transparent` that palette index is skipped.
    pub fn blit(&mut self, which: usize, pixels: &[u8], w: i32, h: i32, mut x: i32, mut y: i32, transparent: Option<u8>) {
        let (mut sx, mut sy, mut cw, mut ch) = (0, 0, w, h);
        if x < 0 {
            sx = -x;
            cw += x;
            x = 0;
        }
        if y < 0 {
            sy = -y;
            ch += y;
            y = 0;
        }
        cw = cw.min(SCREEN_W as i32 - x);
        ch = ch.min(SCREEN_H as i32 - y);
        if cw <= 0 || ch <= 0 {
            return;
        }
        for row in 0..ch {
            let src = &pixels[((sy + row) * w + sx) as usize..][..cw as usize];
            let at = ((y + row) as usize) * SCREEN_W + x as usize;
            let dst = &mut self.pixels[which][at..at + cw as usize];
            match transparent {
                None => dst.copy_from_slice(src),
                Some(t) => {
                    for (d, &v) in dst.iter_mut().zip(src) {
                        if v != t {
                            *d = v;
                        }
                    }
                }
            }
        }
    }

    pub fn blit_sprite(&mut self, which: usize, s: &Sprite, x: i32, y: i32) {
        if !s.pixels.is_empty() {
            self.blit(which, &s.pixels, s.width, s.height, x, y, None);
        }
    }

    /// Mem image numbers start at 1: image n is sprite n - 1 of the archive (a button's
    /// pressed frame and the first stage's map only line up that way).
    pub fn mem_image(&self, index: i32) -> Option<&Sprite> {
        let images = self.mem_images.as_ref()?;
        if index < 1 {
            return None;
        }
        images.sprites.get(index as usize - 1)
    }

    pub fn set_mem_image(&mut self, which: usize, image: i32, mut x: i32, mut y: i32, flags: i32) {
        let images = match &self.mem_images {
            Some(i) => i.clone(),
            None => return,
        };
        let Some(s) = (image >= 1).then(|| images.sprites.get(image as usize - 1)).flatten() else { return };
        if s.pixels.is_empty() {
            return;
        }
        if flags & MI_TOPLEFT == 0 {
            x -= s.center_x;
            y -= s.center_y;
        }
        let (mut sx, mut sy, mut w, mut h) = (0, 0, s.width, s.height);
        let (tx, ty, bx, by) = if flags & MI_CLIP != 0 { self.clip } else { (0, 0, SCREEN_W as i32 - 1, SCREEN_H as i32 - 1) };
        if x < tx {
            sx = tx - x;
            w -= sx;
            x = tx;
        }
        if y < ty {
            sy = ty - y;
            h -= sy;
            y = ty;
        }
        if x + w > bx + 1 {
            w = bx + 1 - x;
        }
        if y + h > by + 1 {
            h = by + 1 - y;
        }
        if w <= 0 || h <= 0 {
            return;
        }
        for row in 0..h {
            let src = &s.pixels[((sy + row) * s.width + sx) as usize..][..w as usize];
            let at = (y + row) as usize * SCREEN_W + x as usize;
            for (d, &v) in self.pixels[which][at..at + w as usize].iter_mut().zip(src) {
                if v != MEM_IMAGE_TRANSPARENT {
                    *d = v;
                }
            }
        }
    }

    /// Palette index closest to a COLORREF (0x00BBGGRR), as GDI picks it.
    pub fn nearest_color(&self, colorref: u32) -> u8 {
        let (r, g, b) = ((colorref & 0xff) as i32, (colorref >> 8 & 0xff) as i32, (colorref >> 16 & 0xff) as i32);
        let mut best = 0;
        let mut best_d = i32::MAX;
        for i in 0..256 {
            let p = &self.palette[i * 3..i * 3 + 3];
            let (dr, dg, db) = (p[0] as i32 - r, p[1] as i32 - g, p[2] as i32 - b);
            let d = dr * dr + dg * dg + db * db;
            if d < best_d {
                best_d = d;
                best = i;
            }
        }
        best as u8
    }
}

pub fn set_mem_images(images: Option<Rc<SpriteArchive>>) {
    with_screen(|s| s.mem_images = images);
}

pub fn get_mem_images() -> Option<Rc<SpriteArchive>> {
    with_screen(|s| s.mem_images.clone())
}

/// A copy of mem image `index` (None when there is none).
pub fn get_mem_image(index: i32) -> Option<Sprite> {
    with_screen(|s| s.mem_image(index).cloned())
}

/// The size of mem image `index` (cheaper than a copy).
pub fn get_mem_image_size(index: i32) -> Option<(i32, i32)> {
    with_screen(|s| s.mem_image(index).filter(|s| !s.pixels.is_empty()).map(|s| (s.width, s.height)))
}

pub fn set_mem_image(which: usize, image: i32, x: i32, y: i32, flags: i32) {
    with_screen(|s| s.set_mem_image(which, image, x, y, flags));
}

pub fn set_clip_limit(x1: i32, y1: i32, x2: i32, y2: i32) {
    with_screen(|s| s.clip = (x1, y1, x2, y2));
}

/// Copies the rectangle (x1,y1)-(x2,y2) of screen src to the same place in dst.
pub fn blt_scr_2_scr(src: usize, dst: usize, x1: i32, y1: i32, x2: i32, y2: i32, _flags: i32) {
    let (w, h) = (SCREEN_W as i32, SCREEN_H as i32);
    if x2 < 0 || x1 > w || y2 < 0 || y1 > h || src == dst {
        return;
    }
    let (x1, y1, x2, y2) = (x1.max(0) as usize, y1.max(0) as usize, x2.min(w) as usize, y2.min(h) as usize);
    if x2 <= x1 {
        return;
    }
    with_screen(|s| {
        for y in y1..y2 {
            let (a, b) = (y * SCREEN_W + x1, y * SCREEN_W + x2);
            let row = s.pixels[src][a..b].to_vec();
            s.pixels[dst][a..b].copy_from_slice(&row);
        }
    });
}

pub fn clear_screen(which: usize, color: u8) {
    screen_clear(which, color);
}

pub fn nearest_color(colorref: u32) -> u8 {
    with_screen(|s| s.nearest_color(colorref))
}

pub fn fill_rect_colorref(which: usize, x: i32, y: i32, w: i32, h: i32, colorref: u32) {
    with_screen(|s| {
        let c = s.nearest_color(colorref);
        for row in y.max(0)..(y + h).min(SCREEN_H as i32) {
            for col in x.max(0)..(x + w).min(SCREEN_W as i32) {
                s.pixels[which][row as usize * SCREEN_W + col as usize] = c;
            }
        }
    });
}

/// Shows the front screen (the original drew straight into the visible surface).
pub fn xvga_show() {
    let (pixels, palette) = with_screen(|s| (s.pixels[SCREEN_FRONT].clone(), s.palette));
    sys_present(&pixels, &palette);
}
