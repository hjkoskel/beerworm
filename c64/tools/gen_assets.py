#!/usr/bin/env python3
"""Converts the game's PNGs and MP3s to C64 data and generates the math tables.

Floats are fine here: this runs on the build machine, the C64 only gets integers.
Needs ffmpeg, no Python packages. Usage: gen_assets.py GFX_DIR SND_DIR OUT_DIR
"""
import math
import subprocess
import sys
from pathlib import Path

# Pepto's C64 palette
PALETTE = [
    (0, 0, 0), (255, 255, 255), (104, 55, 43), (112, 164, 178),
    (111, 61, 134), (88, 141, 67), (53, 40, 121), (184, 199, 111),
    (111, 79, 37), (67, 57, 0), (154, 103, 89), (68, 68, 68),
    (108, 108, 108), (154, 210, 132), (108, 94, 181), (149, 149, 149),
]
WHITE, DARK_GREY = 1, 11
# Warm colors for the border gradient, which has nothing but black, red and yellow
BORDER_COLORS = [0, 2, 7, 8, 9, 10, 11, 12, 15]

# Must match game.h
TICKS_PER_SECOND = 25
PIXELS_PER_TICK = 1.25  # hires pixels: x is half of this in multicolor pixels
TURN_STEP_BINARY_ANGLE = 1001  # 2.4 rad/s, the 0.04 rad per 60 Hz frame of the original
PLAY_COLUMNS = (2, 38)  # cells, end not included
PLAY_ROWS = (3, 23)

SPRITE_WIDTH = 10  # multicolor pixels
SPRITE_MAX_ROWS = 21
PIXEL_ASPECT = 1.2  # how much taller than wide the sprite pixels should look
# Image and sprite color, picked by eye. The rest of each sprite is white and dark grey.
RED, GREEN, ORANGE, LIGHT_RED = 2, 5, 8, 10
BEERS = [("olut_lidl", LIGHT_RED), ("olut_coop", ORANGE), ("olut_heineken", GREEN),
         ("olut_urquell", GREEN), ("olut_alecoq", RED)]
MEDKIT_COLOR = RED

SAMPLE_RATE = 8000  # the player runs at 985248 / 123 = 8010 Hz
COMPANDING = 0.7  # lifts quiet parts, which 4 bits would otherwise lose
SOUNDS = [("drink", "barney"), ("game_over", "doh"), ("wohoo", "wohoo")]


def ffmpeg(*args):
    cmd = ["ffmpeg", "-v", "error", *args, "-"]
    return subprocess.run(cmd, check=True, capture_output=True).stdout


def color_distance(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b))


def sprite_from_png(path, rows, individual):
    """Returns 64 bytes of multicolor sprite. White and dark grey are shared."""
    # Scaling this far averages everything to mid tones, the contrast brings back
    # the highlights and rims for the shared white and dark grey
    raw = ffmpeg("-i", str(path), "-vf",
                 f"scale={SPRITE_WIDTH}:{rows}:flags=area,eq=contrast=1.5:saturation=1.6",
                 "-f", "rawvideo", "-pix_fmt", "rgba")
    assert len(raw) == SPRITE_WIDTH * rows * 4, path
    pixels = [raw[i:i + 4] for i in range(0, len(raw), 4)]

    # Sprite multicolor bits: 01 is $D025, 10 is the sprite's own, 11 is $D026
    choices = [(0b10, PALETTE[individual]), (0b01, PALETTE[WHITE]), (0b11, PALETTE[DARK_GREY])]
    data = bytearray(64)
    for y in range(rows):
        bits = 0
        for x in range(12):
            bits <<= 2
            if x < SPRITE_WIDTH:
                p = pixels[y * SPRITE_WIDTH + x]
                if p[3] >= 128:
                    bits |= min(choices, key=lambda c: color_distance(p[:3], c[1]))[0]
        data[y * 3:y * 3 + 3] = bits.to_bytes(3, "big")
    return bytes(data)


def png_rows(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                          "stream=width,height", "-of", "csv=p=0", str(path)],
                         check=True, capture_output=True, text=True).stdout
    width, height = map(int, out.strip().split(","))
    return SPRITE_WIDTH * 2 * height / width / PIXEL_ASPECT


def border_cells():
    """One (two colors, dither level) per cell outside the play area, row by row."""
    beige, black, yellow, red = (211, 176, 131), (0, 0, 0), (253, 249, 0), (230, 41, 55)
    pairs = []
    for c1 in BORDER_COLORS:
        for c2 in BORDER_COLORS:
            for level in range(5):
                mix = tuple((PALETTE[c1][i] * (4 - level) + PALETTE[c2][i] * level) / 4 for i in range(3))
                pairs.append((mix, c1, c2, level))

    screen, levels = [], []
    for row in range(25):
        for col in range(40):
            if PLAY_COLUMNS[0] <= col < PLAY_COLUMNS[1] and PLAY_ROWS[0] <= row < PLAY_ROWS[1]:
                continue
            u, v = (col + 0.5) / 40, (row + 0.5) / 25
            target = tuple((beige[i] * (1 - u) + red[i] * u) * (1 - v)
                           + (black[i] * (1 - u) + yellow[i] * u) * v for i in range(3))
            # The contrast term stops it from mixing far apart colors into noise
            best = min(pairs, key=lambda p: color_distance(p[0], target)
                       + 0.12 * color_distance(PALETTE[p[1]], PALETTE[p[2]]))
            screen.append(best[1] << 4 | best[2])
            levels.append(best[3])
    return screen, levels


def sound_from_mp3(path):
    raw = ffmpeg("-i", str(path), "-ac", "1", "-ar", str(SAMPLE_RATE), "-f", "u8", "-acodec", "pcm_u8")
    samples = [s - 128 for s in raw]
    mean = sum(samples) / len(samples)
    samples = [s - mean for s in samples]
    peak = max(abs(s) for s in samples) or 1
    threshold = peak * 0.04
    first = next(i for i, s in enumerate(samples) if abs(s) > threshold)
    last = len(samples) - next(i for i, s in enumerate(reversed(samples)) if abs(s) > threshold)
    samples = samples[first:last]

    def to_nibble(s):
        v = abs(s) / peak
        v = min(1.0, v ** COMPANDING)
        v = 7.5 + math.copysign(v * 7.5, s)
        return max(0, min(15, round(v)))

    nibbles = [to_nibble(s) for s in samples]
    # Ramps from and to silence, which is volume 0, to soften the click
    ramp = 24
    nibbles = [round(nibbles[0] * i / ramp) for i in range(ramp)] + nibbles
    nibbles += [round(nibbles[-1] * (ramp - i) / ramp) for i in range(1, ramp + 1)]
    if len(nibbles) % 2:
        nibbles.append(0)
    return bytes(nibbles[i] << 4 | nibbles[i + 1] for i in range(0, len(nibbles), 2))


def byte_lines(data, directive=".byte"):
    return [f"    {directive} " + ",".join(str(v) for v in data[i:i + 16]) for i in range(0, len(data), 16)]


def main():
    gfx_dir, snd_dir, out_dir = (Path(a) for a in sys.argv[1:4])
    asm = ['; Generated by tools/gen_assets.py', '.rodata']
    header = ['// Generated by tools/gen_assets.py', '#ifndef ASSETS_H', '#define ASSETS_H',
              '#include <stdint.h>', f'#define BEER_COUNT {len(BEERS)}',
              f'#define SPRITE_WIDTH {SPRITE_WIDTH}', f'#define TURN_STEP {TURN_STEP_BINARY_ANGLE}']

    def table(name, c_type, count, values, directive):
        asm.extend([f'.export _{name}', f'_{name}:'])
        asm.extend(byte_lines([v & 0xFFFF for v in values], directive))
        header.append(f'extern const {c_type} {name}[{count}];')

    # Hires pixels per tick -> Q8 multicolor pixels in x, Q8 lines in y
    dx = [round(math.cos(i * math.tau / 256) * PIXELS_PER_TICK / 2 * 256) for i in range(256)]
    dy = [round(math.sin(i * math.tau / 256) * PIXELS_PER_TICK * 256) for i in range(256)]
    table("vel_dx", "int16_t", 256, dx, ".word")
    table("vel_dy", "int16_t", 256, dy, ".word")

    # The original adds a random -0.5..0.5 * A radians each 60 Hz frame, with
    # A = 1 - e^(-0.06 * beers). A tick is 2.4 frames, and to have the same random
    # walk the range grows by sqrt(2.4). 64 beers is practically the limit.
    radians_to_angle = 65536 / math.tau
    wobble = [round((1 - math.exp(-0.06 * beers)) * math.sqrt(60 / TICKS_PER_SECOND) * radians_to_angle)
              for beers in range(64)]
    table("wobble_table", "uint16_t", 64, wobble, ".word")

    sprites, colors, heights = bytearray(), [], []
    for name, color in BEERS:
        path = gfx_dir / f"{name}.png"
        rows = min(SPRITE_MAX_ROWS, round(png_rows(path) / 2))  # expanded in y
        sprites += sprite_from_png(path, rows, color)
        colors.append(color)
        heights.append(rows * 2)
    path = gfx_dir / "medkit.png"
    medkit_rows = min(SPRITE_MAX_ROWS, round(png_rows(path)))
    sprites += sprite_from_png(path, medkit_rows, MEDKIT_COLOR)
    table("sprite_data", "uint8_t", len(sprites), sprites, ".byte")
    table("beer_colors", "uint8_t", "BEER_COUNT", colors, ".byte")
    table("beer_heights", "uint8_t", "BEER_COUNT", heights, ".byte")
    header += [f'#define MEDKIT_COLOR {MEDKIT_COLOR}', f'#define MEDKIT_HEIGHT {medkit_rows}']

    screen, levels = border_cells()
    table("border_screen", "uint8_t", len(screen), screen, ".byte")
    table("border_levels", "uint8_t", len(levels), levels, ".byte")

    total = 0
    for name, file_name in SOUNDS:
        data = sound_from_mp3(snd_dir / f"{file_name}.mp3")
        total += len(data)
        table(f"snd_{name}", "uint8_t", len(data), data, ".byte")
        header.append(f'#define SND_{name.upper()}_BYTES {len(data)}')

    header.append('#endif')
    (out_dir / "assets.s").write_text("\n".join(asm) + "\n")
    (out_dir / "assets.h").write_text("\n".join(header) + "\n")
    print(f"sprites {len(sprites)} bytes, sound {total} bytes, border cells {len(screen)}, "
          f"beer heights {heights}, medkit height {medkit_rows}")


if __name__ == "__main__":
    main()
