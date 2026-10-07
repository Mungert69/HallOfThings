# Original maze generator port

This experimental branch replaces the native DFS room generator with a C translation of the published Halls of the Things maze topology routines.

Source routines used as the behavioural reference:

- `Init`
- `Main`
- `Draw`
- `FindMoves`
- `Move`
- `FindNext`
- `FindLinks`
- `Link`
- `Rnd6`
- `Rnd7`
- `Page`
- `WallDefs`
- `ConTab`

The source uses a 16x16 small maze (`Maze`, 256 bytes), two bits per connection direction, and expands it on a 7-character pitch into a 113x113 active playfield.

Deliberate native-port differences:

- the Linux port keeps its existing deterministic xorshift RNG rather than emulating the original four-byte Z80 random routine;
- original closed-door and open-door wall forms are rendered as simple passable wall gaps to preserve the approved modern visual language;
- floor 0 retains the native golden-key sanctuary/game progression while using the original source's special open-plan treatment outside it;
- smooth movement, 8-way aiming, high-resolution rendering and the 32x24 world-cell viewport remain unchanged.
