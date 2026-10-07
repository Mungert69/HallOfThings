# Original Halls of the Things source reference

The Linux port is a clean C reimplementation. The original Halls of the Things
source is used as behavioral and dimensional reference material.

The official Design Design archive identifies the Halls sources as copyright
Simon Brattel, Martin Horsley and Neil Mottershead. Because the archive does not
state an open-source redistribution licence, those copyrighted source files are
not vendored inside this repository.

Run:

```bash
./reference/original/fetch-original-sources.sh
```

That downloads the original files directly from the rights-holder's public
archive into this directory:

- `halls.asm` — consolidated complete Spectrum source
- `halls83_chars.rtf` — original 1983 character set
- `halls83_front.rtf` — original 1983 front end
- `halls83_main.rtf` — original 1983 main game source

Official archive:

https://www.desdes.com/products/oldfiles/

## Reference points currently used by this port

The consolidated `halls.asm` was consulted for:

- original control/action structure and weapon roles;
- player movement timing and direction state (`ManSpeed`, `MoveMan`);
- player rendering dimensions: `PM` chooses character `#A8 + angle*2` and draws
  two vertically stacked character cells, so the original player footprint is
  8x16 pixels;
- Thing/monster character family around `#D0`, giving an 8x8 character-sized
  visual footprint;
- fireball lifetime/search/speed constants;
- lightning lifetime/search/speed and reflection behavior;
- maze/floor generation and room-connection behavior;
- rings, golden-key progression, wounds, magic and scoring behavior.

The Linux renderer intentionally uses newly drawn clean-room bitmaps rather
than copying the original character bitmap bytes. The new sprites use the
original dimensional conventions (8x16 player, 8x8 Things) so their scale and
alignment match the source architecture while the rendering remains original.
