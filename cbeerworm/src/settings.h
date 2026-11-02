// Game settings and command line parsing.
//
// Every value is an integer. Flags keep the decimal notation of the zig and go
// versions (-ws=1.85), and the fractional part is stored in the unit named by
// the field: worm_speed_centi = 185 means 1.85 pixels per frame.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

// Keeps every on-screen coordinate difference inside int16_t, even when the
// worm head has just stepped outside the play area.
#define SETTINGS_MAX_SCREEN 16000
#define SETTINGS_MAX_WORM_WIDTH 180 // 2 * 179^2 still fits in uint16_t

typedef struct {
    uint16_t screen_width;       // -w   pixels
    uint16_t screen_height;      // -h   pixels
    uint16_t length_inc_per_beer; // -i  worm elements

    uint16_t side_margin;        // -ms  pixels
    uint16_t top_margin;         // -mt  pixels
    uint16_t bottom_margin;      // -mb  pixels
    uint16_t bottom_font_size;   // -fb  pixels
    uint16_t top_font_size;      // -ft  pixels

    uint16_t beer_width;         // -b   pixels

    uint16_t worm_speed_centi;   // -ws  1/100 pixels per frame
    uint16_t worm_turn_decimilli; // -wt 1/10000 radians per frame
    uint16_t worm_width;         // -ww  pixels

    uint16_t drunk_factor_centi; // -d   1/100

    uint16_t medkit_visible_ms;  // -kt  milliseconds
    uint16_t medkit_percent;     // -kp  1/100, so -kp=1 is 100
} GameSettings;

GameSettings settings_default(void);

// Fixed-point decimal parser: "1.85" with 2 decimals gives 185. Digits beyond
// the requested precision are dropped. Rejects anything but plain non-negative
// decimals, and values above UINT16_MAX.
bool settings_parse_decimal(const char *text, uint8_t decimals, uint16_t *out);

// Parses "-flag=value" arguments over the defaults. Unknown arguments are
// ignored, like in the other versions. On a bad value or an unusable layout
// prints the reason to stderr and returns false.
bool settings_from_cmdline(int argc, char **argv, GameSettings *out);

#endif
