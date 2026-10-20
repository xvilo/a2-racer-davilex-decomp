# A2 Racer port

A clean C rewrite of A2 Racer on SDL2 that runs on Windows, Linux and macOS. The decompiled modules in `../decompiled/` are the reference; file and function names follow the original sources (`forms.cpp` becomes `menu/forms.c`, `menu_main` stays `menu_main`).

The game's own data is not included. Point the port at the original CD, or at a copy of its files.

## Status

| Part | State |
|---|---|
| Resource archives (`.IND`/`.IMG`), PCX | done |
| Main menu screen and buttons (focus, mouse, keys, hotkeys) | done |
| Main menu: pixel-fade wipe, button entry animation, player info box, stage map, menu music, click sounds | done |
| Options, controls and save slots in the original file formats (`OPTIE.A2R`, `BEDIEN.A2R`, `SLOT.A2R`) | done |
| Start screen: player slots, new player name entry, delete player, logo intro (`--first`) | done |
| Spoken help with the dimmed screen; yes/no and text requesters | done |
| Options menu (display mode, sound, horizon, detail, dashboard, volumes; cancel restores) | done |
| Controls menu (keyboard/joypad/wheel, key configuration with duplicate warning) | done |
| Score screens (overall and per-stage top tens, default names from the original, `SCORE.A2R`) | done |
| New-player screen (switch or create a player; cancel restores the slots) | done |
| Post-race screens (standings, results, `INFIO.INI` hand-over), other wipe types | to do |
| Movies (Smacker) | to do |
| Race (A2SPEL: Direct3D execute buffers → OpenGL) | to do |

## Build

You need CMake ≥ 3.16, a C99 compiler, SDL2 and SDL2_ttf.

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
| `--first` | Starts as the original did right after installing (the logo intro when there are no players) |
| `--script file` | Scripted input for testing: keys, text, clicks and screenshots on a timeline (see `src/platform/sys_sdl.c` and `tests/menu_smoke.txt`) |

## Layout

| Path | Contents |
|---|---|
| `src/main.c` | Entry point; takes the launcher's place (A2.EXE) |
| `src/platform/` | SDL2: window, presenting the 8-bit screen, input, time |
| `src/res/` | Archive and PCX loaders |
| `src/gfx/` | The 640×480 8-bit screens with their palette, and a PNG writer |
| `src/xvga/` | The parts of Davilex's xvga library: save files, fonts, sound buffers |
| `src/menu/` | Forms and menus. `menu_data.[ch]` is generated from `A2MENU.DAT` by `tools/menu_tables.py` |

## Fidelity notes

- **Fonts:** text is drawn like the original's GDI output: Arial Black at the same cell heights, unsmoothed, in the nearest palette colour. The port looks for `ariblk.ttf` in the Windows fonts folder, `/usr/share/fonts/truetype/msttcorefonts` (Debian's `ttf-mscorefonts-installer`) and the macOS font folders.
- **Sprites:** "mem image" numbers start at 1 (image *n* is sprite *n−1* in `A2MRC`), and palette index 255 is transparent.
- **Focus:** as in the original, controls only take the focus on a click, Tab or the menu's own arrow-key handling; hovering does nothing.
- **Name entry:** the default name ("Speler") starts selected, so typing replaces it.
- **Timing:** the original drew wipes as fast as the machine allowed; the port shows each wipe step for 25 ms.
