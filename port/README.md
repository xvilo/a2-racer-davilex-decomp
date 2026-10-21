# A2 Racer port

A clean Rust rewrite of A2 Racer on SDL2 and OpenGL that runs on Windows, Linux and macOS. The decompiled modules in `../decompiled/` are the reference; file and function names follow the original sources (`forms.cpp` becomes `menu/forms.rs`, `menu_main` stays `menu_main`).

The game's own data is not included. Point the port at the original CD, or at a copy of its files.

## Status

| Part | State |
|---|---|
| Resource archives (`.IND`/`.IMG`), PCX | done |
| Main menu screen and buttons (focus, mouse, keys, hotkeys) | done |
| Main menu: pixel-fade wipe, button entry animation, player info box, stage map, menu music, click sounds | done |
| Options, controls and save slots in the original file formats (`OPTIE.A2R`, `BEDIEN.A2R`, `SLOT.A2R`) | done |
| Start screen: player slots, new player name entry, delete player, logo intro when there are no players yet | done |
| Spoken help with the dimmed screen; yes/no and text requesters | done |
| Options menu (display mode, sound, horizon, detail, dashboard, volumes; cancel restores) | done |
| Controls menu (keyboard/joypad/wheel, key configuration with duplicate warning) | done |
| Score screens (overall and per-stage top tens, default names from the original, `SCORE.A2R`) | done |
| New-player screen (switch or create a player; cancel restores the slots) | done |
| Post-race: stage result, overall classification, stage and series high scores, end of the series; `INFIO.INI` hand-over between menu and race | done (tested with `--fake-race`) |
| Movies: own Smacker 2 decoder (video, palette, compressed audio); intro and exit films, skippable as in the original | done |
| Race 3D: the road from the track files, scenery, sky, textures, Direct3D execute buffers → OpenGL 2.1 | done |
| Race: player car and gearbox, chase/cockpit/intro cameras, rivals, police and traffic with their lane-choosing drivers, collisions | done |
| Race: start grid, countdown, ranking and position boards, fines, fuel, time and money, finish and the result in `INFIO.INI` | done |
| Race: dashboard and radar (both sizes), state pictures (pause, out of time/fuel/money, finish) | done |
| Race: bonus items (time, fuel, money) and road works cones | done |
| Race: music, effects, the other cars' engines and sirens, the player's engine with rpm pitch and gear changes | done |
| Race: keyboard, joypad and steering wheel | done (joypad and wheel untested on hardware) |

## Download

Ready-made builds of the latest commit are on the [latest release](https://github.com/xvilo/a2-racer-davilex-decomp/releases/tag/latest): Windows (x86-64, SDL built in), macOS (one universal executable for Apple Silicon and Intel, SDL built in) and Linux (x86-64, SDL built in; runs on glibc 2.35 and later, e.g. Ubuntu 22.04+). Unpack and run `a2racer --data /path/to/A2RACER.BIN` (the CD image, `.bin` or `.iso`), or point `--data` at a folder with the CD's files. The macOS build isn't notarised, so macOS stops a downloaded copy: run `xattr -dr com.apple.quarantine /path/to/a2racer` once after unpacking.

## In the browser

Every push to main also puts a WebAssembly build on [GitHub Pages](https://xvilo.github.io/a2-racer-davilex-decomp/). Choose the CD image (`.bin` or `.iso`) on the page, and optionally `ariblk.ttf` for the menus' text. The image is read in the browser and unpacked once into its IndexedDB storage, together with the saves, so later visits start straight away. Nothing is sent to a server.

Building it needs [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (`emcc` on the PATH) and `rustup target add wasm32-unknown-emscripten`:

```bash
port/web/build.sh                          # into port/target/web
python3 -m http.server -d port/target/web  # then open http://localhost:8000
```

The link flags are in `.cargo/config.toml`. In the browser SDL2 comes from Emscripten's ports, and Emscripten emulates the fixed-function OpenGL on WebGL 1. Asyncify lets the game's own loops, which sleep and poll as on Windows, return control to the browser on each sleep and each frame. Asyncify can't suspend through wasm exception handlers, so the web build is built with `panic=abort`. `web/index.html` is the page: it handles the file choice and the storage and starts the game.

## Build

You need Rust (stable, via [rustup](https://rustup.rs)). The race needs an OpenGL 2.1 driver at run time; the port loads the GL functions through SDL, so nothing links against OpenGL. Text is drawn with [fontdue](https://crates.io/crates/fontdue), so SDL2_ttf isn't needed.

```bash
# With SDL2 installed (Debian/Ubuntu: sudo apt install libsdl2-dev; macOS: brew install sdl2):
cargo build --release --manifest-path port/Cargo.toml
# Or build SDL2 from source and link it in (needs CMake and a C compiler; what the releases use):
cargo build --release --features static --manifest-path port/Cargo.toml
port/target/release/a2racer --data /path/to/A2Racer-CD
```

| Option | Does |
|---|---|
| `--data <dir or image>` | Where the game files are (the environment variable `A2_DATA` also works): a folder with the CD's files, or the CD image itself (`.bin` with raw 2352-byte sectors, or `.iso`). An image is unpacked once into `disc/` in the user's data folder |
| `--save <dir>` | Where options and save slots go. Default: the user's data folder (`~/.local/share/Davilex/A2 Racer/` on Linux, `%APPDATA%\Davilex\A2 Racer\` on Windows). The files are byte-compatible with the original's |
| `--fonts <dir>` | A folder with `ariblk.ttf` (Arial Black, the menus' font), if it isn't in the system font folders |
| `--scale N` | Opens the window at N × 640×480. The window can also be resized freely and keeps 4:3 |
| `--screenshot file.png` | Draws the main menu, saves it and exits, with no window |
| `--race N` | Starts stage N (1–6) straight away with the saved player's settings, without the menu |
| `--race-view N` | A free fly-through along stage N's road (a test of the 3D code) |
| `--fake-race N` | Skips the race: "drives" each one at position N (1–6) with made-up times, to test the screens after a race |
| `--script file` | Scripted input for testing: keys, text, clicks and screenshots on a timeline (see `src/platform/sys.rs` and `tests/menu_smoke.txt`) |

## Layout

| Path | Contents |
|---|---|
| `src/main.rs` | Entry point; takes the launcher's place (A2.EXE) |
| `src/platform/` | SDL2: window, presenting the 8-bit screen, OpenGL, input, time |
| `web/` | The browser version's page and build script |
| `src/res/` | Archive, PCX, disc image and Smacker loaders |
| `src/gfx/` | The 640×480 8-bit screens with their palette, wipes, and a PNG writer |
| `src/xvga/` | The parts of Davilex's xvga library: save files, INI files, fonts, sound buffers, joysticks |
| `src/movie/` | The movie player (A2MOVIE.DAT) |
| `src/menu/` | Forms and menus. `menu_data.rs` is generated from `A2MENU.DAT` by `tools/menu_tables.py` |
| `src/race/` | The race (A2SPEL.DAT), its state in one `Race` struct: `race.rs` (main.cpp, objio.cpp, setracer.cpp), `weg.rs` (the road), `opponent.rs`/`driver.rs` (cars and their drivers), `player.rs` (mycontro.cpp), `camera.rs`, `statobj.rs` (scenery), `bonus.rs`, `pion.rs`, `hud.rs` (dashboard and radar), `sndfx.rs`, and the 3D layer: `math3d.rs`, `object3d.rs`, `render.rs`, `texture.rs` |

## Fidelity notes

- **Fonts:** text is drawn like the original's GDI output: Arial Black at the same cell heights, unsmoothed, in the nearest palette colour. The port looks for `ariblk.ttf` in the Windows fonts folder, `/usr/share/fonts/truetype/msttcorefonts` (Debian's `ttf-mscorefonts-installer`) and the macOS font folders.
- **Sprites:** "mem image" numbers start at 1 (image *n* is sprite *n−1* in `A2MRC`), and palette index 255 is transparent.
- **Focus:** as in the original, controls only take the focus on a click, Tab or the menu's own arrow-key handling; hovering does nothing.
- **Name entry:** the default name ("Speler") starts selected, so typing replaces it.
- **Wipes:** only the pixel fade (type 3) is used by the game, so the other wipe types are not ported.
- **INFIO.INI:** the menu and the race were separate programs that talked through `INFIO.INI`. The port keeps that file (in the save folder) and the launcher's loop: menu, race, the winner's film after a won final stage, menu again.
- **Timing:** the original drew wipes as fast as the machine allowed; the port shows each wipe step for 25 ms.
- **Race frame rate:** the original ran one race frame per screen refresh, as fast as the machine allowed, and everything moves per frame. The port runs the race at a fixed 30 frames a second.
- **Race drawing:** Direct3D 5 execute buffers become OpenGL 2.1 vertex arrays with the same render states: no lighting, the texture replaces the colour, nearest filtering (bilinear in the "3D card" display mode), palette index 255 transparent. The dashboard and radar are drawn into an 8-bit layer with the original's sprites and laid over the 3D picture.
- **Race quirks kept:** the police and traffic always use `POLICE0`/`CIVIL0` from `A2.INI`; the police chase count compares the wrong index; bonus items and cones spin faster and slower because each frame's turn is added to the last.
- **Joysticks:** as in the original, the joypad's stick and first buttons stand for the steering and pedal keys, and the wheel's x axis steers directly. Unlike the original, the keyboard keeps working when the chosen joystick isn't there.
- **Testing aids:** with the environment variable `A2_AUTOPILOT=1` the player's car drives itself, below the speed that alerts the police, changing lanes when blocked; its tank stays full (`A2_AUTOPILOT=fast`: full throttle; `A2_AUTOPILOT=nofuel`: the tank runs dry). With `--race N`, `A2_RACE_REPEAT=n` runs the race n times in one process, as the launcher does when the player goes on to the next race.
- **One process:** the original ran the menu and each race as separate programs. The port runs them in one, so the race resets everything it caches (model frames, fines, cameras) at its start and hands the menu its sprites back at its end.
