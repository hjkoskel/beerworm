# Kaljamato for the Commodore 64

Port of the game to a PAL C64, written in C for [cc65](https://cc65.github.io/) with small parts in assembly. It uses the same images and sounds as the other versions, including digitized sound effects.

## Building

Needs cc65, Python 3 and ffmpeg. The converter uses no Python packages. VICE is for `make run`.

~~~sh
make          # build/kaljamato.d64 and the files on it
make run      # starts the disk in VICE, with the loading screen
make run-prg  # starts only the game in VICE, skipping the loading
make test     # game logic tests, 6502 code run in sim65
~~~

On a real machine, or in VICE by hand: `LOAD"*",8,1` and `RUN`.

## Controls

| Input | function |
|-------|----------|
| joystick port 2 left, or Z | turn left |
| joystick port 2 right, or X | turn right |
| P | pause |
| RUN/STOP | end the game |
| any key or fire on the game over screen | new game |

## How it works

**Loading screen.** `src/splashload.kla` is shown while the game loads. The first file on the disk is `src/boot.s`, which loads the picture's colors, then the bitmap with the screen on so that the picture builds up, then the game behind it. The picture stays for at least 5 s, which matters on fast drives and emulators; a key or fire skips the rest of that wait. The game loads to `$0801`, over the boot program, so the last part of the loader runs from the cassette buffer. A red border means that a file did not load. With the plain KERNAL loader on a 1541, the whole thing takes a bit over a minute.

**Screen.** Multicolor bitmap, 160 x 200, in VIC bank 2. Beers and the medkit are hardware sprites, so they are never redrawn. The worm is blue, light blue and white, which every cell in the play area has available.

**Worm.** The game runs at 25 ticks per second, on every second frame. Each tick only draws the new head and clears the tail, 3 x 5 pixels each. Self-collision checks the bitmap under the new head and ignores the latest 8 elements, because comparing the head with every element would take too long.

**Math.** No floats, no multiplication per tick. The heading is a 16-bit angle, whose high byte selects the velocity from a table of 8.8 fixed-point values. `tools/gen_assets.py` generates the tables on the build machine, and it may use floats since none of them reach the C64.

**Sound.** The SID volume register plays 4-bit samples at 8 kHz, and the game stops while a sound plays, about 0.4 to 0.6 s. A CIA timer is polled for the rate, so the VIC stealing cycles does not change the pitch. All three voices hold a steady output for the volume to scale. It works on both SID models: on the 8580 emulation in VICE it makes the sound about 5 times stronger than without it, though the 6581 is still about 4 times louder.

**Memory.**

| Address | Content |
|---------|---------|
| `$033C-$03FB` | end of the loader, only while loading |
| `$0801-$3F9C` | game program, including 6274 bytes of sound |
| up to `$7FFF` | free, and the C stack at the top |
| `$8000-$83FF` | screen RAM and sprite pointers |
| `$8400-$857F` | sprites |
| `$A000-$BF3F` | bitmap, under the BASIC ROM |
| `$C000-$CFFF` | worm ring buffer |

## Differences from the other versions

- Sounds stop the game, by choice, for better sound quality.
- Color bands stay in place on the worm's body. The other versions recolor the whole worm every frame.
- The worm's length is capped at 2047 elements, reached after about 60 beers, and the score at 9999.
- The game over screen starts a new game instead of exiting, and the random numbers carry on, so the next game is different.
- Sizes and speeds are scaled to the screen, and the settings flags are compile-time constants in `src/game.h` and `tools/gen_assets.py`.
- PAL only. On NTSC the game runs 20% too fast.

## cc65 note

cc65 2.18 with plain `-O` generates wrong code when a `uint16_t` parameter is masked twice in a function, as in `RING_X[i & MASK]` followed by `RING_Y[i & MASK]`: the second access uses 0 for the high byte. The Makefile uses `-Oi`, which compiled it correctly, and `src/game.c` also avoids the pattern.
