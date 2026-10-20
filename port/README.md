# A2 Racer port

A clean C rewrite of A2 Racer on SDL2 that runs on Windows, Linux and macOS. The decompiled modules in `../decompiled/` are the reference; file and function names follow the original sources (`forms.cpp` becomes `menu/forms.c`, `menu_main` stays `menu_main`).

The game's own data is not included. Point the port at the original CD, or at a copy of its files.

## Status

| Part | State |
|---|---|
| Resource archives (`.IND`/`.IMG`), PCX | done |
| Main menu screen and buttons (focus, mouse, keys, hotkeys) | done |
| Menu details: wipes, button entry animation, slot info, music, help voices | to do |
| Other menus (controls, options, scores, new player, save slots) | to do |
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
| `--scale N` | Opens the window at N × 640×480. The window can also be resized freely and keeps 4:3 |
| `--screenshot file.png` | Draws the main menu, saves it and exits, with no window |

## Layout

| Path | Contents |
|---|---|
| `src/main.c` | Entry point; takes the launcher's place (A2.EXE) |
| `src/platform/` | SDL2: window, presenting the 8-bit screen, input, time |
| `src/res/` | Archive and PCX loaders |
| `src/gfx/` | The 640×480 8-bit screens with their palette, and a PNG writer |
| `src/menu/` | Forms and menus. `menu_data.[ch]` is generated from `A2MENU.DAT` by `tools/menu_tables.py` |
