# Halls of the Things — original-maze experiment v2

This is an alternate comparison build. It keeps the current smooth high-resolution game but replaces the native DFS maze generator with a C translation of the published 16x16 Halls maze topology algorithm.


Native C/SDL2 Linux reimplementation of *Halls of the Things*.

## Project layout

```text
.
├── CMakeLists.txt
├── include/
│   └── hall_of_things/
│       └── game.h
├── src/
│   ├── game.c
│   ├── main.c
│   └── sprites.h
├── tests/
│   └── test_game.c
├── reference/
│   └── original/
│       ├── README.md
│       └── fetch-original-sources.sh
└── LICENSE-NOTICE.md
```

Build artifacts belong only in `build/` and are not included in the source archive.

## Debian 13

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/halls-original-maze-v3
```

For a repeatable maze:

```bash
./build/halls-original-maze-v3 12345
```

## Controls

- Arrow keys: move and set the persistent aiming direction
- Hold two arrow keys together for diagonals, e.g. Up+Right
- Releasing the keys stops movement but keeps the last aiming direction
- `S` / Space: sword in the current aiming direction
- `A` / Enter: arrow in the current aiming direction
- `F`: homing fireball; it acquires the nearest Thing and steers as it moves
- `L`: lightning in the current aiming direction; it reflects from walls
- Items auto-pick up when you move onto them if you have capacity; `K` remains as an optional manual pickup key
- `D`: drop treasure
- `H`: show/hide the controls overlay
- `C`: show/hide the cheat overlay (golden-key floor and win sequence)
- `E`: heal
- `R`: test refill
- `1`: status
- `Esc`: quit



## Visual renderer

The main renderer uses a 1280x720 logical canvas designed for a laptop display. The whole playfield is black open space with thin green wall centre-lines; doorways are plain gaps in those lines. The permanent HUD is a narrow icon strip for rings, magic, arrows, health, floor and score. Sprites are deliberately small relative to the rooms so the game keeps the open, ricochet-friendly feel. This experimental build uses the original 16x16 room topology on the original 7-cell pitch, expanded into the 113x113 world.

Press `1` for status, `H` for the controls overlay, and `C` for the cheat/win overlay.

## Small release binary

Release builds default to size-oriented compiler/linker settings (`-Os`, section garbage collection and symbol stripping). This affects only the final `halls-original-maze-v3` executable; CMake metadata, tests and the static library in `build/` are development artifacts and are not part of the game executable. Disable this with `-DHALLS_SMALL_BINARY=OFF` if you want an unstripped release binary for debugging.

## Smooth movement

The SDL frontend now interpolates player motion over each logical grid step and follows the interpolated position with the camera. Collision, pickups and combat remain grid-based and deterministic. Projectiles are also interpolated between their previous and current logical cells, so arrows, fireballs and lightning travel smoothly without changing their gameplay timing.


## Sprite alignment with the Spectrum source

The renderer now follows the dimensions implied by the original character renderer: the player is an **8x16** bitmap (two stacked Spectrum character cells) and Things are **8x8**. The Linux port keeps eight-way facing, so the four diagonal player frames are new additions; all of the bitmaps in `src/sprites.h` are clean-room artwork rather than copied original character bytes.

This keeps characters small relative to the high-resolution logical cells while retaining the smooth open-screen renderer.

## Original source reference folder

`reference/original/` documents every original source set currently used as a reference and includes `fetch-original-sources.sh`. Run that script to place the official Design Design source files directly in the project. They are not redistributed in this ZIP because the archive labels them as copyrighted and does not state an open redistribution licence.

## Cheat ring locator

Press `C` to show the cheat overlay. It now lists the number of uncollected rings remaining on every floor 1-7, the golden-key floor, and the win sequence.

## Fidelity pass

The current renderer keeps the smooth high-resolution native presentation but
uses the original game's character scale as its visual reference: the player is
8x16, Things and ordinary objects are 8x8, and the sword is shown as an animated
sweep. Projectile lifetimes/search ranges and relative speeds are aligned with
the published source where practical, while the native eight-way aiming and
smooth interpolation remain deliberate extensions.

See `reference/original/USED_SOURCE.md` for the source-to-port mapping.


## Spectrum-style viewport

The high-resolution SDL renderer deliberately limits the visible playfield to the original ZX Spectrum **32 x 24 character-cell field of view** (256 x 192 pixels on the original machine). The Linux window remains 1280x720 and each logical cell is rendered at higher resolution, so movement/camera interpolation remain smooth while the player sees the same amount of world geometry that fitted on a Spectrum screen. The compact HUD is outside the clipped playfield and does not consume a world row.


## Experimental original maze generator

Floors 1-7 use a direct C translation of the original `Main` / `Draw` / `FindMoves` / `Move` / `FindNext` / `FindLinks` / `Link` topology algorithm. The generator uses a 16x16 one-byte-per-room small maze and the original `ConTab` probability values, then expands that topology at a 7-cell pitch into the existing 113x113 world. Floor 0 remains the special open-plan level, matching the original source's separate treatment, with the native golden-key sanctuary retained for the current complete game loop.

The random-number generator is intentionally still the native port RNG, so a given Linux seed is repeatable but does not produce the exact same byte sequence as a Spectrum run. See `reference/original/ORIGINAL_MAZE_PORT.md`.


## Experimental start floor

This comparison build defaults to **floor 1**, because floors 1–7 are the levels that use the original 16x16 room-maze renderer. The original source deliberately treats floor 0 as a special open-plan level, which made the first experimental build appear to have no maze at startup.

Run with a fixed seed:

```bash
./build/halls-original-maze-v3 12345
```

Optionally select a starting floor with a second argument:

```bash
./build/halls-original-maze-v3 12345 0   # special open-plan floor 0
./build/halls-original-maze-v3 12345 1   # first generated maze floor
./build/halls-original-maze-v3 12345 7   # top generated maze floor
```


## v3 smooth Things

The player and projectiles were already visually interpolated between logical grid cells. v3 also interpolates Thing/monster movement between their logical cells, preserving the existing AI and collision timing while removing visible cell-by-cell jumps.
