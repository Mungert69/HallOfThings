# Halls of the Things — original-maze v2

A native C/SDL2 Linux reimplementation of *Halls of the Things*, the
1983 ZX Spectrum dungeon adventure by Design Design
(Simon Brattel, Martin Horsley and Neil Mottershead).

## Why I ported it

*Halls of the Things* was my favourite game on the ZX Spectrum. I
wanted to play it on Linux, and I was curious how its maze generator
actually worked, so I wrote this clean-room port in C. It is a love
letter to the original — every line of code and every bitmap here is
newly written, using the published Spectrum source only as behavioural
and dimensional reference.

## What this build is

This is the **original-maze v2** experiment: it keeps the smooth,
high-resolution game but replaces the native DFS maze generator with a
direct C translation of the published 16x16 Halls maze topology
algorithm. Floors 1–7 are grown with the original `Main` / `Draw` /
`FindMoves` / `Move` / `FindNext` / `FindLinks` / `Link` routines and
the original `ConTab` connection probabilities, then expanded at the
original 7-cell pitch into the 113x113 world. Floor 0 keeps the
special open-plan treatment (and the native golden-key sanctuary) from
the original source.

The goal is the original one: explore the maze floors, collect the
seven magic rings, find the golden key and escape.

## Features

- **Faithful maze topology** — original 16x16 one-byte-per-room
  generator expanded into the 113x113 world, with deterministic seeds
  for repeatable floors
- **Spectrum viewport** — the visible playfield is limited to the
  original ZX Spectrum 32x24 character-cell field of view (256x192
  pixels), drawn at high resolution in a 1280x720 window
- **Smooth movement** — the player and projectiles interpolate between
  logical cells while collision, pickups and combat stay grid-based
  and deterministic
- **The original arsenal** — animated sword sweep, arrows, a homing
  fireball that steers toward the nearest Thing, and lightning that
  reflects off walls, with original lifetimes, ranges and relative
  speeds wherever practical
- **Original character scale** — the player is an 8x16 bitmap (two
  stacked Spectrum character cells) and Things are 8x8; the four
  diagonal frames are new additions for the eight-way aiming
- **Compact HUD** — rings, magic, arrows, health, floor and score in
  a narrow icon strip outside the clipped playfield
- **Overlays** — `1` status, `H` controls, `C` cheat locator
  (uncollected ring counts per floor, the golden-key floor and the win
  sequence)

## Build (Debian 13)

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/halls-original-maze-v2
```

Run with a fixed seed, optionally picking a starting floor:

```bash
./build/halls-original-maze-v2 12345     # repeatable maze, default start floor 1
./build/halls-original-maze-v2 12345 0   # special open-plan floor 0
./build/halls-original-maze-v2 12345 7   # top generated maze floor
```

Release builds default to size-oriented settings (`-Os`, section garbage
collection, symbol stripping); disable with `-DHALLS_SMALL_BINARY=OFF`.

## Controls

| Key | Action |
| --- | --- |
| Arrow keys | move and set the persistent aiming direction |
| Two arrows together | diagonals, e.g. Up+Right |
| `S` / Space | sword in the aiming direction |
| `A` / Enter | arrow in the aiming direction |
| `F` | homing fireball (2 magic; acquires the nearest Thing) |
| `L` | lightning in the aiming direction; reflects from walls |
| `K` | manual pickup (items also auto-pick up when you walk onto them) |
| `D` | drop treasure |
| `E` | heal (30 magic) |
| `H` | show/hide the controls overlay |
| `C` | show/hide the cheat overlay |
| `1` | status |
| `R` | test refill |
| `Esc` | quit |

Releasing the keys stops movement but keeps the last aiming direction.

## Project layout

```text
.
├── CMakeLists.txt
├── include/hall_of_things/game.h   # game state, tiles, constants
├── src/game.c                      # grid logic: maze gen, movement, combat
├── src/main.c                      # SDL2 renderer, input, camera, HUD
├── src/sprites.h                   # clean-room bitmap artwork
├── tests/test_game.c               # connectivity and state tests (CTest)
└── reference/original/             # mapping to the published Spectrum source
    ├── README.md
    ├── USED_SOURCE.md
    ├── ORIGINAL_MAZE_PORT.md
    └── fetch-original-sources.sh   # downloads the rights-holder's archive files
```

Build artifacts belong only in `build/` and are not part of the source
tree.

## Fidelity and deliberate extensions

The port aligns projectile lifetimes, search ranges, relative speeds,
damage, magic/arrow/wound limits and starting values with the published
source where practical. Deliberate native extensions: smooth sub-cell
rendering and camera motion, arrow-key movement with simultaneous-key
diagonals, persistent eight-way aim, the high-resolution open playfield
and the compact graphical HUD.

The Linux port's random-number generator is the native xorshift RNG, so
a given seed is repeatable on Linux but does not reproduce the exact
byte sequence of a Spectrum run. See
`reference/original/ORIGINAL_MAZE_PORT.md` and
`reference/original/USED_SOURCE.md` for the full source-to-port mapping.

## Original sources

`reference/original/` documents every original source set used as a
reference. Run `./reference/original/fetch-original-sources.sh` to
download the official Design Design files (the consolidated `halls.asm`,
the 1983 character set and front end) directly from the rights-holder's
public archive. They are not vendored here because the archive does not
state an open redistribution licence.

## Licence

See [LICENSE-NOTICE.md](LICENSE-NOTICE.md). This reimplementation is new
C code and clean-room artwork; *Halls of the Things* and the original
source/assets remain the property of their respective copyright holders.
