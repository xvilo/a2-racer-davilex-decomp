//! What is drawn over the 3D picture (dashbrd.cpp, radar.cpp, menuanim.cpp, and the
//! pictures objio.cpp's race states show): the dashboard or the small indicators, the
//! radar, the time, money and place, and the countdown and messages. The original drew
//! these 8-bit sprites from GAMESPR onto its back buffer; the port draws them onto an
//! 8-bit layer laid over the 3D frame.

use std::path::Path;
use std::rc::Rc;

use crate::gfx::screen::{get_mem_images, set_mem_images, with_screen, SCREEN_BACK};
use crate::platform::sys::{sys_gl_overlay, SCREEN_H, SCREEN_W};
use crate::race::driver::{DRIVER_POLICE, DRIVER_RIVAL};
use crate::race::math3d::{cos_a, sin_a};
use crate::race::Race;
use crate::res::archive::SpriteArchive;
use crate::res::pcx::pcx_decode;

/// anims in A2SPEL.DAT: the race's sprite animations, as GAMESPR image numbers.
const RACE_ANIMS: [&[i32]; 35] = [
    /*  0 */ &[1],
    /*  1 */ &[56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236],
    /*  2 */ &[91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110],
    /*  3 */ &[249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268],
    /*  4 */ &[76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90],
    /*  5 */ &[4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 111, 112, 113, 114, 115, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176],
    /*  6 */ &[116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216],
    /*  7 */ &[136, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156],
    /*  8 */ &[177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196],
    /*  9 */ &[137, 243, 244, 245, 246, 247],
    /* 10 */ &[248, 237, 238, 239, 240, 241],
    /* 11 */ &[12],
    /* 12 */ &[242],
    /* 13 */ &[21],
    /* 14 */ &[31, 22, 23, 24, 25, 26, 27, 28, 29, 30],
    /* 15 */ &[42],
    /* 16 */ &[32, 33, 34, 35, 36, 37, 38, 39, 40, 41],
    /* 17 */ &[48, 49, 50, 51, 43],
    /* 18 */ &[52, 44, 45, 46, 47],
    /* 19 */ &[299, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298, 284],
    /* 20 */ &[283, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 53],
    /* 21 */ &[300],
    /* 22 */ &[302],
    /* 23 */ &[324],
    /* 24 */ &[323],
    /* 25 */ &[331],
    /* 26 */ &[332],
    /* 27 */ &[312, 313, 314, 315, 316, 317, 318, 319, 320, 303],
    /* 28 */ &[54, 301, 304, 305, 306, 307, 308, 309, 310, 311],
    /* 29 */ &[326],
    /* 30 */ &[325],
    /* 31 */ &[328],
    /* 32 */ &[329],
    /* 33 */ &[327],
    /* 34 */ &[330],
];

pub struct Hud {
    gamespr: Option<Rc<SpriteArchive>>,
    /// The menu's sprites, back in place after the race (a separate program in the
    /// original).
    menu_images: Option<Rc<SpriteArchive>>,
    /// surscrn.pcx
    dashboard_picture: Vec<u8>,
    race_palette: [u8; 768],
    /// F5
    pub radar_on: bool,
}

impl Default for Hud {
    fn default() -> Hud {
        Hud { gamespr: None, menu_images: None, dashboard_picture: vec![0xff; SCREEN_W * SCREEN_H], race_palette: [0; 768], radar_on: true }
    }
}

impl Race {
    pub fn init_hud(&mut self, data_dir: &Path) -> bool {
        self.hud.menu_images = get_mem_images();
        let Some(gamespr) = SpriteArchive::open(data_dir, "GAMESPR") else {
            eprintln!("cannot read GAMESPR");
            return false;
        };
        let gamespr = Rc::new(gamespr);
        set_mem_images(Some(gamespr.clone()));
        self.hud.gamespr = Some(gamespr);
        // The race's palette comes with the dashboard picture.
        if let Some(img) = self.files.race_file("surscrn.pcx").and_then(pcx_decode) {
            self.hud.race_palette = img.palette;
            let (w, h) = (img.width as usize, img.height as usize);
            for y in 0..h.min(SCREEN_H) {
                let n = w.min(SCREEN_W);
                self.hud.dashboard_picture[y * SCREEN_W..y * SCREEN_W + n].copy_from_slice(&img.pixels[y * w..y * w + n]);
            }
        }
        true
    }

    pub fn exit_hud(&mut self) {
        if self.hud.gamespr.take().is_some() {
            set_mem_images(self.hud.menu_images.take());
        }
    }

    /// A frame of one of the race's anims at (x, y); flags as set_mem_image's.
    pub fn hud_image(&self, anim: i32, frame: i32, x: i32, y: i32, flags: i32) {
        let Some(images) = RACE_ANIMS.get(anim as usize) else { return };
        if frame < 0 || frame as usize >= images.len() {
            return;
        }
        with_screen(|s| s.set_mem_image(SCREEN_BACK, images[frame as usize], x, y, flags));
    }

    /// Starts a frame's layer.
    pub fn hud_begin(&self) {
        with_screen(|s| {
            let p = &mut s.pixels[SCREEN_BACK];
            if self.dashboard_on_screen {
                // The dashboard below the 3D picture (which is 640x300 then).
                p[..SCREEN_W * 300].fill(0xff);
                p[SCREEN_W * 300..].copy_from_slice(&self.hud.dashboard_picture[SCREEN_W * 300..]);
            } else {
                p.fill(0xff);
            }
            s.palette = self.hud.race_palette;
        });
    }

    /// Lays the layer over the 3D frame.
    pub fn hud_end(&self) {
        let pixels = with_screen(|s| s.pixels[SCREEN_BACK].clone());
        sys_gl_overlay(&pixels, &self.hud.race_palette, 0xff);
    }

    // --- radar.cpp ---------------------------------------------------------------------

    pub fn draw_radar(&self) {
        if !self.hud.radar_on {
            return;
        }
        let (rx, ry, w, h, big) = if self.dashboard_on_screen { (0x1e6, 0x179, 0x3a, 0x3a, false) } else { (6, 0x166, 0x74, 0x74, true) };
        let (cx, cy) = (w / 2, w / 2);
        let n = if big { 4 } else { 2 };
        let a = -self.player_heading;
        let (c, s) = (cos_a(a), sin_a(a));
        let mut dots: Vec<(i32, i32, u8)> = Vec::new();
        let mut chunk = self.opp.data[self.pools.objects3d[self.player_index].opponent as usize].chunk - 10;
        for _ in 0..0x14 {
            if chunk >= 0 && chunk < self.weg.chunk_count - 1 {
                for lane in 0..0xb {
                    let op = self.get_map_occupant(chunk, lane);
                    if op == -1 || op == 0 {
                        continue;
                    }
                    let o = &self.opp.data[op as usize];
                    let dx = (o.x - self.player_x) as i32 as f32 / 12.0;
                    let dz = (o.z - self.player_z) as i32 as f32 / 12.0;
                    let x = (cx as f32 + (dx * c - dz * s)) as i32;
                    let y = (h as f32 - (cy as f32 + dz * c + dx * s)) as i32;
                    // Civilians dark, rivals and police brighter.
                    let colour = match o.field_e4 {
                        DRIVER_RIVAL => 0x3c,
                        DRIVER_POLICE => 0x58,
                        _ => 0x0a,
                    };
                    if 0 < x && x < w - 4 && 0 < y && y < h - 4 {
                        dots.push((x, y, colour));
                    }
                }
            }
            chunk += 1;
        }
        let dash = self.dashboard_on_screen;
        with_screen(|scr| {
            let dst = &mut scr.pixels[SCREEN_BACK];
            let mut put = |x: i32, y: i32, c: u8| dst[((ry + y) as usize) * SCREEN_W + (rx + x) as usize] = c;
            // Background: without the dashboard every other pixel of every other row black
            // (the rest see-through), with it all black.
            for y in 0..h {
                for x in 0..w {
                    if dash || (y % 2 == 1 && x % 2 == 0) {
                        put(x, y, 0);
                    }
                }
            }
            // The player: two pixels wide, two (four) high.
            for y in 0..n {
                for x in 0..n {
                    put(cx + x, cy + y, 0x14);
                }
            }
            for (x, y, colour) in dots {
                for yy in 0..n {
                    for xx in 0..n {
                        put(x + xx, y + yy, colour);
                    }
                }
            }
        });
    }

    // --- dashbrd.cpp -------------------------------------------------------------------

    fn speed_frame(&self) -> i32 {
        if self.player_speed.abs() == 0.0 {
            return 0;
        }
        ((self.player_speed * 2.666_666_7) as i32).abs().min(0x27)
    }

    fn fuel_frame(&self) -> i32 {
        if self.fuel == 0 {
            return 0;
        }
        ((self.fuel as f32 * 0.066_666_67) as i32).abs().min(0x13)
    }

    fn draw_race_position(&mut self) {
        let place = self.compute_race_ranking().map(|r| r[0]).unwrap_or(6) - 1;
        self.hud_image(10, place.clamp(0, 5), 0x24e, 0x19, 0);
        self.hud_image(12, 0, 0x1fc, 0xf, 2);
    }

    fn draw_time_left(&self) {
        let t = self.time_left;
        let (h, m, s) = (t / 3600, t / 60 % 60, t % 60);
        let (x, y) = (0x42, 0x18);
        self.hud_image(0xd, 0, 0x10, 0x10, 2);
        for (d, dx) in [(h / 10, 0), (h % 10, 0x14), (m / 10, 0x39), (m % 10, 0x4e), (s / 10, 0x71), (s % 10, 0x86)] {
            self.hud_image(0xe, d, x + dx, y, 0);
        }
    }

    fn draw_money(&self) {
        let (x, y, step) = (0x42, 0x36, 0x14);
        let m = self.money;
        self.hud_image(0x1d, 0, 0x10, 0x2e, 2);
        self.hud_image(0xe, m / 1000, x, y, 0);
        self.hud_image(0xe, m % 1000 / 100, x + step, y, 0);
        self.hud_image(0xe, m % 100 / 10, x + step * 2, y, 0);
        self.hud_image(0xe, m % 10, x + step * 3, y, 0);
    }

    pub fn draw_dashboard(&mut self) {
        if !self.dashboard_on_screen {
            self.hud_image(6, self.speed_frame(), 0x1e4, 0x166, 2);
            self.hud_image(8, self.fuel_frame(), 0x1b0, 0x193, 2);
        } else {
            self.hud_image(0, 0, 0, 0x10b, 2);
            self.hud_image(4, self.player.player_steering_position() / 2, 0x2c, 0x15c, 2);
            self.hud_image(3, self.fuel_frame(), 0x124, 0x148, 2);
            self.hud_image(1, self.speed_frame(), 0xd7, 0x140, 2);
            let rpm = self.player.engine_rpm;
            let rev = if rpm == 0 { 0 } else { ((rpm as f32 * 0.285_714_3) as i32).abs() };
            self.hud_image(2, rev.min(0x13), 0x87, 0x13f, 2);
        }
        self.draw_race_position();
        self.draw_time_left();
        self.draw_money();
    }
}
