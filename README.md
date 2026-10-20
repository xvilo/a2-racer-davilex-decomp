# A2 Racer — static decompilation

Static reverse engineering of the Dutch CD-ROM release of **A2 Racer** (Davilex Entertainment, 1997), using [REA](https://github.com/morluto/rea) with Ghidra as the decompiler backend.

This repo holds **only tooling and decompiled output**. The CD dump and the files unpacked from it are **not** committed. Keep them outside the repo, by default in `../A2Racer-dump/`.

## Layout

| Path | Contents |
|---|---|
| `tools/` | Scripts to unpack the dump and to decompile the game modules |
| `decompiled/<module>/` | Per module: `decompiled.c` (every function), `functions.tsv`, `strings.tsv`, and REA's `overview.json` |
| `decompiled/<module>/src/` | The same functions split into the original source files (`opponent.c`, `camera.c`, …; C runtime under `crt/`). `sources.tsv` says how each function was placed: A2MENU's debug info, a name match with A2MENU, a `.cpp` name in an assert string, or its neighbours |
| `build/` | Ghidra projects, REA snapshots and logs (not committed) |

## The dump (not in this repo)

Expected input is `A2RACER_AMST.BIN`, with SHA-256 `a6710ba40ea107cd822d5622d0cb564d02540e16dadf43ecb68202a60e3279ab`. It has 46,245 raw 2352-byte sectors (CD-ROM XA Mode 2, one data track) and came without a `.cue` file.

Unpack it next to the repo:

```bash
D=../A2Racer-dump
python3 -I tools/bin2iso.py $D/A2RACER_AMST.BIN $D/A2RACER_AMST.iso
python3 -I tools/isoextract.py $D/A2RACER_AMST.iso $D/disc
```

`bin2iso.py` takes the 2048-byte user data from each sector. `isoextract.py` reads the Joliet directory tree and strips the `;1` version suffixes.

## Decompile

```bash
export JAVA_HOME=~/opt/jdk-21.0.12.1+1
export GHIDRA_INSTALL_DIR=~/opt/ghidra_12.1.4_PUBLIC
tools/decompile_all.sh                 # all modules
tools/decompile_all.sh A2MENU.DAT      # one module
A2_DISC=/elsewhere/disc tools/decompile_all.sh
```

For each module the script runs `rea analyze` (Ghidra provider, with a snapshot in `build/`). It then runs Ghidra headless with these scripts:

| Script | Does |
|---|---|
| `ApplyWatcom.java` | Applies names, signatures and structures from `symbols/`. It also adds Watcom's register calling conventions, names shared tails, and defines short strings and float constants |
| `InferSignatures.java` | For functions with no known signature, finds the register parameters (EAX, EDX, EBX, ECX) from the machine code, then the return type from the call sites. Each function gets `__watcall0`…`__watcall4` by its register-parameter count, since Watcom preserves every argument register a function doesn't take. Each variadic call gets its real argument count |
| `AutoNameVariables.java` | Names variables from argument roles, call results, loop counters and types |
| `ExportDecompiled.java` | Writes `decompiled.c`, `functions.tsv` (with each prototype) and `strings.tsv` |

`tools/split_sources.py` then splits the C export into the original source files.

## Game modules

All are 32-bit x86 PE files built with Watcom C/C++ in 1997.

| File | Role |
|---|---|
| `A2.EXE` | Launcher: reads `infio.ini`, then starts the modules below |
| `A2MENU.DAT` | Menus, options, controls, save slots, scores |
| `A2SPEL.DAT` | The race itself (Direct3D) |
| `A2MOVIE.DAT` | Smacker video player for `*.SMK` |

## Toolchain

- REA (`rea-agents`) 6.7.0
- Ghidra 12.1.4
- Temurin JDK 21.0.12
