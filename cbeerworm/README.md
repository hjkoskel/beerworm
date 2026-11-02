# Kaljamato in C

C port of the zig game in the parent folder, with [raylib](https://www.raylib.com/) and without floating point. The go version in `../gobeer` was used as a second reference.

## Building

Needs raylib and a C23 compiler with `#embed` (gcc 15 or newer). All media files are compiled into the executable.

~~~sh
make          # builds build/kaljamato
make run ARGS="-w=800 -h=600"
make test     # game logic tests, no window needed
~~~

The Makefile compiles with `-mgeneral-regs-only`, so any floating point added to the game code is a compile error. If your compiler or CPU does not have that flag, use `make NOFLOAT=`.

## Usage

| Key   | function   |
|-------|------------|
| left  | turn left  |
| right | turn right |
|   P   | pause |
|   F   | toggle fullscreen |
|  ESC  | end the game |

Flags are the same as in the other versions and still take decimals. They are stored as integers in the listed unit, and digits beyond it are dropped.

| flag | default | stored as | range | explanation |
|------|---------|-----------|-------|-------------|
|-w    |1024  | pixels    | 1-16000 | horizontal resolution |
|-h    |768   | pixels    | 1-16000 | vertical resolution |
|-i    |80    | elements  | 0-65535 | worm growth per beer |
|-ms   |50    | pixels    | 0-16000 | side border |
|-mt   |100   | pixels    | 0-16000 | header bar |
|-mb   |50    | pixels    | 0-16000 | footer |
|-fb   |40    | pixels    | 1-16000 | footer font size |
|-ft   |100   | pixels    | 1-16000 | title font size |
|-b    |60    | pixels    | 1-16000 | width of beers and medkit |
|-ws   |1.85  | 1/100     | 0.01-100 | pixels per frame |
|-wt   |0.04  | 1/10000   | 0-6.5535 | radians turn per frame |
|-ww   |17    | pixels    | 1-180 | worm width |
|-d    |6     | 1/100     | 0-655.35 | how fast alcohol hits |
|-kt   |8000  | ms        | 0-65535 | how long the medkit is visible |
|-kp   |0.3   | 1/100     | 0-1 | probability of a medkit per beer |

## Integer math

| zig version | here |
|-------------|------|
| direction vector, rotated with `cos` and `sin` | `uint16_t` binary angle (65536 is a full circle, wraps by itself), Q15 sine table with interpolation |
| `f32` position | `int16_t` pixels, plus a `uint8_t` Q8 fraction for the head only |
| `f32` speed | `int16_t` Q8 pixels per frame |
| `ArrayList` with `orderedRemove(0)` | ring buffer of `Point16`, indexed by a wrapping `uint16_t` |
| `1 - e^(-d * factor)` | table with interpolation, computed only when drunkness changes |
| `f32` drunkness | `uint16_t` in 1/100, printed as `%d.%02d` |
| `milliTimestamp` | frame counter, 8000 ms is 480 frames |
| 64-bit prng | 16-bit xorshift, with a fixed seed like the other versions |

`int32_t`/`uint32_t` remain only where a product of two 16-bit values needs them: the Q15 multiply, sine interpolation, and unit conversions and table lookups done when loading or drinking. The body collision check stays in 16 bits, which is why the worm width is limited to 180.

raylib calls with float parameters are replaced with integer ones:

| zig version | here |
|-------------|------|
| `DrawLineEx` | tinted disc texture, generated at startup, with `DrawTexture` |
| `DrawTextureEx` with a scale | `ImageResize` when loading, then `DrawTexture` |
| `DrawRectangleGradientEx` | the same quad with `rlBegin`, `rlColor4ub` and `rlVertex2i` |
| `DrawRectangleRec` | `DrawRectangle` |
| `WaitTime` in pause and game over | states in the main loop, paced by `SetTargetFPS` |

raylib itself uses floats internally.

## Differences from the zig version

- The worm is drawn from discs, so it has a rounded look and the color bands have curved edges.
- Headings have 1/256 pixel steps in the velocity, about 0.12 degrees at default speed. Turning uses the exact angle, without the zig version's `y` calculated from the already rotated `x`.
- The worm stops growing at 65535 elements.
- The medkit timer counts frames, so it stops during pause and follows the game speed if the frame rate drops.
- `-kt` works. The zig version fails to parse it because of a wrong offset.
- Closing the window quits right away, also on the game over screen. ESC goes to the game over screen as before.
- Bad flag values and margins that leave no play area give an error message.
- Loaded images, textures, waves, sounds and the audio device are released on exit.
