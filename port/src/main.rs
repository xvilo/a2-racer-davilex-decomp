//! A2 Racer port: entry point. Takes the place of A2.EXE (the launcher), which ran the
//! menu (A2MENU.DAT) and race (A2SPEL.DAT) as separate programs; the start-up follows
//! A2MENU.DAT's WinMain and win_open_menu.

mod game;
mod gfx;
mod menu;
mod movie;
mod platform;
mod race;
mod res;
mod xvga;

// SDL built in (the `static` feature) reads the registry for audio device names and
// mouse settings; sdl2-sys doesn't link the library that has those functions.
#[cfg(all(windows, feature = "static"))]
#[link(name = "advapi32")]
extern "system" {}

use std::path::{Path, PathBuf};
use std::rc::Rc;

use game::Game;
use gfx::png::png_write_indexed;
use gfx::screen::{clear_screen, set_mem_images, with_screen, SCREEN_FRONT};
use menu::infio::Infio;
use menu::m_main::{MENU_EXIT, MENU_START_RACE};
use menu::Menu;
use movie::movies::play_movie;
use platform::sys::{sys_init, sys_quit_requested, sys_set_screenshot, sys_shutdown, SysOptions, SCREEN_H, SCREEN_W};
use res::archive::{NamedArchive, SpriteArchive};
use res::discimage::{disc_image_unpack, is_disc_image};
use xvga::fileio::fileio_init;
use xvga::font::{font_init, font_shutdown, xvga_load_font, xvga_set_font_bkmode, xvga_set_font_colors};
use xvga::inifile::{read_ini_value_setting, set_ini_filename};
use xvga::sound::{xvga_exit_sound, xvga_init_sound};

fn save_front_screen(path: &Path) {
    with_screen(|s| {
        let _ = png_write_indexed(path, &s.pixels[SCREEN_FRONT], SCREEN_W, SCREEN_H, &s.palette);
    });
}

fn usage() {
    eprintln!(
        "usage: a2racer --data <disc image or directory> [options]
  --data <dir|image>   the A2 Racer CD image (.bin/.iso) or a copy of its files
  --save <dir>         where options and save slots go (default: user data folder)
  --fonts <dir>        folder with ariblk.ttf (Arial Black) if not found otherwise
  --scale <n>          window size as a multiple of 640x480
  --race <stage>       drive a stage (1..6) straight away, without the menu
  --fake-race <pos>    instead of driving, finish each race at this position
                       (1..6) with made-up times, to test what follows
  --race-view <stage>  fly along a stage's road (1..6; tests the 3D code)
  --screenshot <file>  open the main menu, save it as PNG and exit
  --script <file>      scripted input for testing (see platform/sys.rs)"
    );
}

/// win_open_menu's fonts: Arial Black in five sizes, transparent background.
fn load_menu_fonts() {
    const HEIGHT: [i32; 5] = [0xd, 0x14, 0x17, 0x12, 0x12];
    for (i, &h) in HEIGHT.iter().enumerate() {
        xvga_load_font(i, "Arial Black", h, false, false);
        xvga_set_font_colors(i, 0, 0xffffff);
        xvga_set_font_bkmode(i, 1);
    }
}

/// win_open_menu: the settings, save slots, scores and INFIO.INI from disk.
fn win_open_menu(m: &mut Menu, g: &mut Game) {
    clear_screen(0, 0);
    clear_screen(2, 0);
    clear_screen(1, 0);
    m.init_optie();
    m.init_bedien();
    m.init_slotdbase();
    m.init_scoredbase();
    g.infio.init_infio();
    let last = m.get_last_saved_slot();
    if m.slot_list[last as usize].used != 0 {
        m.set_active_slot(last);
    }
    m.set_help_key(0x0d);
    m.set_help_key(0x20);
    m.set_help_key(0x1b);
}

/// win_close_menu: everything back to disk, for the race and the next menu run.
fn win_close_menu(m: &mut Menu, g: &Game) {
    clear_screen(0, 0);
    clear_screen(2, 0);
    clear_screen(1, 0);
    m.save_optie();
    m.save_bedien();
    m.update_active_slot_optie();
    m.save_slotdbase();
    m.save_scoredbase();
    g.infio.save_infio();
    m.clear_help_keys();
}

/// win_handle_menu: one run of the menu. Coming back from a race, handle_game shows the
/// result first and goes on to the main menu, or to the player choice when the series
/// is over. MENU_START_RACE or MENU_EXIT.
fn win_handle_menu(m: &mut Menu, g: &mut Game, first: bool) -> i32 {
    win_open_menu(m, g);
    if (first || !m.handle_game(g)) && !m.menu_start(g, first) {
        win_close_menu(m, g);
        return MENU_EXIT;
    }
    let r = m.menu_main(g);
    m.reset_infio(g);
    win_close_menu(m, g);
    r
}

/// A2SPEL.DAT's place: the race. With --fake-race, a finished race is written to
/// INFIO.INI instead, the player at `position` and the rivals in the other places.
fn run_race(g: &mut Game, position: i32) {
    if !(1..=6).contains(&position) {
        race::race::race_main(g);
        return;
    }
    g.infio.init_infio();
    let i = &mut g.infio;
    i.race_finished = 1;
    i.position = position as i16;
    i.time = 240 + position * 7;
    let mut place = 1;
    for r in 0..5 {
        if place == position {
            place += 1;
        }
        i.racer[r] = place as i16;
        i.time_racer[r] = 240 + place * 7;
        place += 1;
    }
    i.money += 500;
    i.save_infio();
}

struct Args {
    data: Option<PathBuf>,
    save: Option<PathBuf>,
    fonts: Option<PathBuf>,
    scale: u32,
    script: Option<PathBuf>,
    race: Option<i32>,
    race_view: Option<i32>,
    fake_race: i32,
    screenshot: Option<PathBuf>,
}

fn parse_args() -> Option<Args> {
    let mut a = Args {
        data: std::env::var_os("A2_DATA").map(PathBuf::from),
        save: None,
        fonts: None,
        scale: 1,
        script: None,
        race: None,
        race_view: None,
        fake_race: 0,
        screenshot: None,
    };
    let mut it = std::env::args().skip(1);
    while let Some(opt) = it.next() {
        let mut value = || it.next();
        match opt.as_str() {
            "--data" => a.data = Some(value()?.into()),
            "--save" => a.save = Some(value()?.into()),
            "--fonts" => a.fonts = Some(value()?.into()),
            "--scale" => a.scale = value()?.parse().ok()?,
            "--script" => a.script = Some(value()?.into()),
            "--race" => a.race = Some(value()?.parse::<i32>().ok()? - 1),
            "--race-view" => a.race_view = Some(value()?.parse::<i32>().ok()? - 1),
            "--fake-race" => a.fake_race = value()?.parse().ok()?,
            "--screenshot" => a.screenshot = Some(value()?.into()),
            _ => return None,
        }
    }
    Some(a)
}

fn main() {
    std::process::exit(run());
}

fn run() -> i32 {
    let Some(args) = parse_args() else {
        usage();
        return 2;
    };
    let Some(mut data_dir) = args.data.clone() else {
        usage();
        return 2;
    };
    // A CD image instead of a folder: its files are unpacked once into the user's data
    // folder.
    if is_disc_image(&data_dir) {
        let pref = sdl2::filesystem::pref_path("Davilex", "A2 Racer").map(PathBuf::from).unwrap_or_else(|_| ".".into());
        let disc = pref.join("disc");
        if let Err(e) = disc_image_unpack(&data_dir, &disc) {
            eprintln!("{e}");
            return 1;
        }
        data_dir = disc;
    }
    let (Some(menu_files), Some(menu_sprites)) =
        (NamedArchive::open(&data_dir, "A2MRCS"), SpriteArchive::open(&data_dir, "A2MRC"))
    else {
        eprintln!("cannot read A2MRCS/A2MRC archives in {}", data_dir.display());
        return 1;
    };
    let opt = SysOptions { scale: args.scale, hidden: args.screenshot.is_some(), script: args.script.clone() };
    if let Err(e) = sys_init("A2 Racer", &opt) {
        eprintln!("{e}");
        return 1;
    }
    sys_set_screenshot(save_front_screen);
    fileio_init(args.save.clone());
    let mut g = Game {
        data_dir,
        menu_files: Rc::new(menu_files),
        menu_sprites: Rc::new(menu_sprites),
        infio: Infio::default(),
    };
    set_mem_images(Some(g.menu_sprites.clone())); // forms_init
    font_init(args.fonts.clone());
    let mut m = Menu::new();

    // WinMain: sound (40 buffers) and the fonts, kept for the whole session.
    if args.screenshot.is_some() || xvga_init_sound(0x28, Some(g.menu_files.clone())).is_err() {
        m.mysnd_state = 1;
    }
    load_menu_fonts();

    let mut result = 0;
    if let Some(stage) = args.race_view {
        result = race::race_view::race_view(&g, stage.max(0) as usize);
    } else if let Some(stage) = args.race {
        // A2_RACE_REPEAT (testing): several races in one run, as the launcher's loop does.
        let repeat: i32 = std::env::var("A2_RACE_REPEAT").ok().and_then(|v| v.parse().ok()).unwrap_or(1);
        for _ in 0..repeat {
            if sys_quit_requested() {
                break;
            }
            win_open_menu(&mut m, &mut g);
            m.reset_infio(&mut g);
            g.infio.stage = stage as i16;
            g.infio.save_infio();
            result = race::race::race_main(&mut g);
            println!("race: finished {}, position {}, time {} s", g.infio.race_finished, g.infio.position, g.infio.time);
        }
    } else if let Some(path) = &args.screenshot {
        win_open_menu(&mut m, &mut g);
        m.mm_open_screen(&g);
        save_front_screen(path);
    } else {
        // The launcher (A2.EXE): the intro film, then the menu and the race in turn
        // until the player leaves the menu, then the exit film. The launcher passes
        // FIRST on the first menu run of a session: with no players yet, that shows the
        // logo intro.
        let mut first = true;
        play_movie(&g.data_dir, "a2intro.smk");
        while win_handle_menu(&mut m, &mut g, first) == MENU_START_RACE && !sys_quit_requested() {
            first = false;
            run_race(&mut g, args.fake_race);
            // After the last stage, won: the winner's film.
            set_ini_filename("INFIO.INI");
            let stage = read_ini_value_setting("GAME", "ETAPPE");
            let finished = read_ini_value_setting("GAME", "RACE_KLAAR");
            let position = read_ini_value_setting("GAME", "POSITIE");
            if stage == 5 && finished != 0 && position != 0 {
                play_movie(&g.data_dir, "a2win.smk");
            }
        }
        if !sys_quit_requested() {
            play_movie(&g.data_dir, "a2exit.smk");
        }
    }
    font_shutdown();
    xvga_exit_sound();
    sys_shutdown();
    result
}
