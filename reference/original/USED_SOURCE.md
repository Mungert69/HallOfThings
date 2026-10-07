# Original source mapping used by the native port

The native port is a new C implementation.  The original Design Design sources
are used as behavioural and dimensional references; they are not compiled into
the Linux executable.

Run `./reference/original/fetch-original-sources.sh` if you want the published
sources alongside this project locally.

Reference points used by the current port:

- `PM` / `PMD`: player is two stacked 8x8 character cells and has four original cardinal facings.
- `MonsterTypes`, `MoveMonsters`: 32 Things, D0..D7 visual family, movement types and 30..49 wound variation.
- `ConTab`: floor-dependent room-connection probabilities.
- `FireBall`: cost 2 magic, life 50, search range 40, damage 40, target tracking.
- `Lightning`: cost 1 magic, life 100, damage 40 and reflection on wall collision.
- `ArrowLifeMan` / `ArrowSpeed`: player arrow lifetime and relative projectile speed.
- `TryHealing`: 30 magic and 16..31 wounds healed.
- `InitGame`: starting magic 120, arrows 255, wounds 0.
- `MaxWounds`, `MaxMagic`, `MaxTreasure`: 127, 255 and 5 respectively.
- `Swd*Tab`: the original sword is an animated sweep; the native renderer keeps a smooth five-position sweep.

Deliberate native extensions retained:

- smooth sub-cell rendering and camera motion;
- arrow-key movement with simultaneous-key diagonals;
- persistent eight-way aim;
- high-resolution open black playfield and compact graphical HUD;
- auto-pickup and testing/help overlays.
