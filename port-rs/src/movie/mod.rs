//! Movies (A2MOVIE.DAT, smacker.cpp): the intro, the end-of-game and the exit film.
//! The original switched to 320x200 (MOVIE.INI's default) or stretched the picture to
//! 640x480; the port always stretches it over the 640x480 screen. Escape, Enter, Space
//! or a mouse click stops it (init_smacker's keys).

use std::path::Path;

use crate::gfx::screen::{clear_screen, with_screen, xvga_show, SCREEN_FRONT};
use crate::platform::sys::{sys_poll, sys_sleep, sys_ticks, SysInput, KEY_ENTER, KEY_ESCAPE, KEY_SPACE, SCREEN_H, SCREEN_W};
use crate::res::archive::read_file_ci;
use crate::res::smacker::Smacker;
use crate::xvga::sound::{play_pcm, stop_sound_buffer};

fn movie_key_pressed(input: &SysInput) -> bool {
    input.quit || input.mouse_clicked || [KEY_ESCAPE, KEY_ENTER, KEY_SPACE].contains(&input.key)
}

/// The decoded frame, stretched to the whole screen (nearest pixel).
fn show_frame(s: &Smacker) {
    let (w, h) = (s.width as usize, s.height as usize);
    with_screen(|scr| {
        for y in 0..SCREEN_H {
            let row = &s.pixels[(y * h / SCREEN_H) * w..];
            for x in 0..SCREEN_W {
                scr.pixels[SCREEN_FRONT][y * SCREEN_W + x] = row[x * w / SCREEN_W];
            }
        }
        scr.palette = s.palette;
    });
    xvga_show();
}

/// Plays a movie from the disc; Some(true) when the player stopped it, Some(false) when
/// it ran to the end, None when it cannot be played.
pub fn play_movie(data_dir: &Path, name: &str) -> Option<bool> {
    let Some(data) = read_file_ci(data_dir, name) else {
        eprintln!("movie: {name} not found");
        return None;
    };
    let Some(mut s) = Smacker::open(data) else {
        eprintln!("movie: {name} is not a Smacker file");
        return None;
    };
    let snd = s.decode_audio().map(|(pcm, rate, channels)| play_pcm(&pcm, rate, channels, 0)).unwrap_or(-1);
    let start = sys_ticks();
    let mut input = SysInput::default();
    let mut stopped = false;
    let mut f = 0;
    while f < s.frames && !stopped {
        if !s.next_frame() {
            break;
        }
        show_frame(&s);
        // SmackWait: hold each frame until its time has come.
        let due = start.wrapping_add(((f + 1) as f64 * s.frame_ms) as u32);
        loop {
            sys_poll(&mut input);
            if movie_key_pressed(&input) {
                stopped = true;
                break;
            }
            sys_sleep(2);
            if (sys_ticks().wrapping_sub(due) as i32) >= 0 {
                break;
            }
        }
        f += 1;
    }
    if snd >= 0 {
        stop_sound_buffer(snd);
    }
    clear_screen(SCREEN_FRONT, 0);
    xvga_show();
    Some(stopped)
}
