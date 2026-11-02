#include "settings.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

GameSettings settings_default(void)
{
    return (GameSettings){
        .screen_width = 1024,
        .screen_height = 768,
        .length_inc_per_beer = 80,
        .side_margin = 50,
        .top_margin = 100,
        .bottom_margin = 50,
        .bottom_font_size = 40,
        .top_font_size = 100,
        .beer_width = 60,
        .worm_speed_centi = 185,
        .worm_turn_decimilli = 400,
        .worm_width = 17,
        .drunk_factor_centi = 600,
        .medkit_visible_ms = 8000,
        .medkit_percent = 30,
    };
}

bool settings_parse_decimal(const char *text, uint8_t decimals, uint16_t *out)
{
    uint32_t value = 0;
    uint8_t frac_digits = 0;
    bool seen_digit = false;
    bool seen_point = false;

    for (const char *c = text; *c != '\0'; c++) {
        if (*c == '.') {
            if (seen_point) {
                return false;
            }
            seen_point = true;
            continue;
        }
        if (*c < '0' || *c > '9') {
            return false;
        }
        seen_digit = true;
        if (seen_point) {
            if (frac_digits == decimals) {
                continue; // beyond our precision
            }
            frac_digits++;
        }
        value = value * 10 + (uint32_t)(*c - '0');
        if (value > UINT16_MAX) {
            return false;
        }
    }
    if (!seen_digit) {
        return false;
    }
    for (; frac_digits < decimals; frac_digits++) {
        value *= 10;
        if (value > UINT16_MAX) {
            return false;
        }
    }
    *out = (uint16_t)value;
    return true;
}

typedef struct {
    const char *prefix;
    size_t field; // offset into GameSettings
    uint8_t decimals;
    uint16_t min;
    uint16_t max; // in stored units
} FlagSpec;

#define FLAG(prefix, field, decimals, min, max) \
    {prefix, offsetof(GameSettings, field), decimals, min, max}

static const FlagSpec FLAGS[] = {
    FLAG("-w=", screen_width, 0, 1, SETTINGS_MAX_SCREEN),
    FLAG("-h=", screen_height, 0, 1, SETTINGS_MAX_SCREEN),
    FLAG("-i=", length_inc_per_beer, 0, 0, UINT16_MAX),
    FLAG("-ms=", side_margin, 0, 0, SETTINGS_MAX_SCREEN),
    FLAG("-mt=", top_margin, 0, 0, SETTINGS_MAX_SCREEN),
    FLAG("-mb=", bottom_margin, 0, 0, SETTINGS_MAX_SCREEN),
    FLAG("-fb=", bottom_font_size, 0, 1, SETTINGS_MAX_SCREEN),
    FLAG("-ft=", top_font_size, 0, 1, SETTINGS_MAX_SCREEN),
    FLAG("-b=", beer_width, 0, 1, SETTINGS_MAX_SCREEN),
    FLAG("-ws=", worm_speed_centi, 2, 1, 10000), // at most 100 pixels per frame
    FLAG("-wt=", worm_turn_decimilli, 4, 0, UINT16_MAX),
    FLAG("-ww=", worm_width, 0, 1, SETTINGS_MAX_WORM_WIDTH),
    FLAG("-d=", drunk_factor_centi, 2, 0, UINT16_MAX),
    FLAG("-kt=", medkit_visible_ms, 0, 0, UINT16_MAX),
    FLAG("-kp=", medkit_percent, 2, 0, 100),
};

bool settings_from_cmdline(int argc, char **argv, GameSettings *out)
{
    GameSettings s = settings_default();

    for (int i = 1; i < argc; i++) {
        for (size_t f = 0; f < sizeof(FLAGS) / sizeof(FLAGS[0]); f++) {
            const FlagSpec *flag = &FLAGS[f];
            size_t prefix_len = strlen(flag->prefix);
            if (strncmp(argv[i], flag->prefix, prefix_len) != 0) {
                continue;
            }
            uint16_t value;
            if (!settings_parse_decimal(argv[i] + prefix_len, flag->decimals, &value)
                || value < flag->min || value > flag->max) {
                fprintf(stderr, "invalid value in %s\n", argv[i]);
                return false;
            }
            memcpy((char *)&s + flag->field, &value, sizeof(value));
            break;
        }
    }

    if ((uint32_t)s.side_margin * 2 >= s.screen_width
        || (uint32_t)s.top_margin + s.bottom_margin >= s.screen_height) {
        fprintf(stderr, "margins leave no play area\n");
        return false;
    }
    *out = s;
    return true;
}
