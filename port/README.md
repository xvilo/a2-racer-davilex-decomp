# A2 Racer port

A clean C rewrite of A2 Racer on SDL2 and OpenGL that runs on Windows, Linux and macOS. The decompiled modules in `../decompiled/` are the reference; file and function names follow the original sources (`forms.cpp` becomes `menu/forms.c`, `menu_main` stays `menu_main`).

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

Ready-made builds of the latest commit are on the [latest release](https://github.com/xvilo/a2-racer-davilex-decomp/releases/tag/latest): Windows (x86-64, with its DLLs), macOS (Apple Silicon, with its libraries) and Linux (x86-64; needs SDL2 and SDL2_ttf from the system, e.g. `sudo apt install libsdl2-2.0-0 libsdl2-ttf-2.0-0`). Unpack and run `a2racer --data /path/to/A2Racer-CD`. The macOS build isn't signed: the first time, open it with right-click → Open, or run `xattr -dr com.apple.quarantine a2racer`.

## Build

You need CMake ≥ 3.16, a C99 compiler, SDL2 and SDL2_ttf. The race needs an OpenGL 2.1 driver at run time; the port loads the GL functions through SDL, so nothing links against OpenGL.

```bash
# Debian/Ubuntu: sudo apt install cmake libsdl2-dev libsdl2-ttf-dev
# macOS:         brew install cmake sdl2 sdl2_ttf
# Windows:       vcpkg install sdl2 sdl2-ttf, then pass -DCMAKE_TOOLCHAIN_FILE=…/vcpkg.cmake
cmake -S port -B port/build
cmake --build port/build
port/build/a2racer --data /path/to/A2Racer-CD
```

| Option | Does |
|---|---|
| `--data <dir>` | Where the game files are (the environment variable `A2_DATA` also works) |
| `--save <dir>` | Where options and save slots go. Default: the user's data folder (`~/.local/share/Davilex/A2 Racer/` on Linux, `%APPDATA%\Davilex\A2 Racer\` on Windows). The files are byte-compatible with the original's |
| `--fonts <dir>` | A folder with `ariblk.ttf` (Arial Black, the menus' font), if it isn't in the system font folders |
| `--scale N` | Opens the window at N × 640×480. The window can also be resized freely and keeps 4:3 |
| `--screenshot file.png` | Draws the main menu, saves it and exits, with no window |
| `--race N` | Starts stage N (1–6) straight away with the saved player's settings, without the menu |
| `--race-view N` | A free fly-through along stage N's road (a test of the 3D code) |
| `--fake-race N` | Skips the race: "drives" each one at position N (1–6) with made-up times, to test the screens after a race |
| `--script file` | Scripted input for testing: keys, text, clicks and screenshots on a timeline (see `src/platform/sys_sdl.c` and `tests/menu_smoke.txt`) |

## Layout

| Path | Contents |
|---|---|
| `src/main.c` | Entry point; takes the launcher's place (A2.EXE) |
| `src/platform/` | SDL2: window, presenting the 8-bit screen, input, time |
| `src/res/` | Archive, PCX and Smacker loaders |
| `src/gfx/` | The 640×480 8-bit screens with their palette, and a PNG writer |
| `src/xvga/` | The parts of Davilex's xvga library: save files, fonts, sound buffers |
| `src/movie/` | The movie player (A2MOVIE.DAT) |
| `src/menu/` | Forms and menus. `menu_data.[ch]` is generated from `A2MENU.DAT` by `tools/menu_tables.py` |
| `src/race/` | The race (A2SPEL.DAT): `race.c` (main.cpp, objio.cpp, setracer.cpp), `weg.c` (the road), `opponent.c`/`driver.c` (cars and their drivers), `player.c` (mycontro.cpp), `camera.c`, `statobj.c` (scenery), `bonus.c`, `pion.c`, `hud.c` (dashboard and radar), `sndfx.c`, and the 3D layer: `math3d.c`, `object3d.c`, `render.c`, `texture.c` |

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
- **Testing aid:** with the environment variable `A2_AUTOPILOT=1` the player's car drives itself, below the speed that alerts the police, changing lanes when blocked; its tank stays full.
