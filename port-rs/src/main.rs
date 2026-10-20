mod gfx;
mod movie;
mod platform;
mod res;
mod xvga;

use std::path::{Path, PathBuf};

use gfx::png::png_write_indexed;
use gfx::screen::{with_screen, SCREEN_FRONT};
use platform::sys::{sys_init, sys_set_screenshot, SysOptions, SCREEN_H, SCREEN_W};

fn save_front_screen(path: &Path) {
    with_screen(|s| {
        let _ = png_write_indexed(path, &s.pixels[SCREEN_FRONT], SCREEN_W, SCREEN_H, &s.palette);
    });
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let data = PathBuf::from(&args[1]);
    let script = args.get(2).map(PathBuf::from);
    sys_init("A2 Racer", &SysOptions { scale: 1, hidden: false, script }).unwrap();
    sys_set_screenshot(save_front_screen);
    if let Err(e) = xvga::sound::xvga_init_sound(0x28, None) {
        eprintln!("sound: {e}");
    }
    println!("{:?}", movie::play_movie(&data, "a2intro.smk"));
}
