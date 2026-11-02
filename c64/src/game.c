#include "game.h"

#include "gfx.h"

// Ring buffer of the worm, above the program in memory: see c64.cfg
#define RING_MASK 2047
#define RING_X ((uint8_t *)0xC000)
#define RING_Y ((uint8_t *)0xC800)

// Limits for the center of the head, keeping the whole worm in the play area
#define HEAD_MIN_X (PLAY_LEFT + 1)
#define HEAD_MAX_X (PLAY_LEFT + PLAY_WIDTH - 2)
#define HEAD_MIN_Y (PLAY_TOP + 2)
#define HEAD_MAX_Y (PLAY_TOP + PLAY_HEIGHT - 3)

// The stamps of this many latest elements can overlap the new head
#define RECENT_ELEMENTS 8

#define MAX_WOBBLE_INDEX 63

Game game;
uint16_t game_rng = 0xACE1; // fixed start like in the other versions

// Band colors from ring index. Multicolor bitmap colors, see draw_play_area().
static const uint8_t BANDS[16] = {1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 2, 2, 2};

uint16_t rng_next(void)
{
    uint16_t x = game_rng;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    game_rng = x;
    return x;
}

uint8_t __fastcall__ worm_x_at(uint16_t ring_index)
{
    return RING_X[ring_index & RING_MASK];
}

uint8_t __fastcall__ worm_y_at(uint16_t ring_index)
{
    return RING_Y[ring_index & RING_MASK];
}

// Ring accesses go through this static on purpose. cc65 2.18 -O gets the high byte
// wrong when a uint16_t parameter is masked twice in a function, as in
// RING_X[i & RING_MASK] followed by RING_Y[i & RING_MASK].
static uint16_t masked;

static void stamp(uint16_t ring_index, uint8_t color)
{
    masked = ring_index & RING_MASK;
    gfx_x = (uint8_t)(RING_X[masked] - STAMP_WIDTH / 2);
    gfx_y = (uint8_t)(RING_Y[masked] - STAMP_HEIGHT / 2);
    gfx_w = STAMP_WIDTH;
    gfx_h = STAMP_HEIGHT;
    gfx_color = color;
    gfx_rect();
}

static void stamp_element(uint16_t ring_index)
{
    stamp(ring_index, BANDS[(uint8_t)ring_index & 15]);
}

static void set_drunk(uint8_t drunk)
{
    game.drunk = drunk;
    game.wobble_amp = wobble_table[drunk < MAX_WOBBLE_INDEX ? drunk : MAX_WOBBLE_INDEX];
}

void game_init(void)
{
    game.pos_x = (uint16_t)(PLAY_LEFT * 2) << 8;
    game.pos_y = (uint16_t)(PLAY_TOP * 2) << 8;
    game.angle = 0x4000; // downwards
    game.head = 1;
    game.count = 2;
    game.length = WORM_START_LENGTH;
    RING_X[0] = RING_X[1] = PLAY_LEFT * 2;
    RING_Y[0] = RING_Y[1] = PLAY_TOP * 2;
    stamp_element(1);
    set_drunk(0);

    game.beer_x = 80;
    game.beer_y = 100;
    game.beer_index = (uint8_t)(rng_next() % BEER_COUNT);
    game.medkit_ticks_left = 0;
    game.score = 0;
    game.max_drunk = 0;
}

// Is the pixel covered by the stamp of a recent element? Those always overlap.
static uint8_t in_recent_stamp(uint8_t x, uint8_t y)
{
    uint8_t i;
    uint16_t index = game.head;
    uint8_t checks = game.count < RECENT_ELEMENTS ? (uint8_t)game.count : RECENT_ELEMENTS;

    for (i = 0; i < checks; i++) {
        masked = index & RING_MASK;
        if ((uint8_t)(x - RING_X[masked] + STAMP_WIDTH / 2) < STAMP_WIDTH
            && (uint8_t)(y - RING_Y[masked] + STAMP_HEIGHT / 2) < STAMP_HEIGHT) {
            return 1;
        }
        index--;
    }
    return 0;
}

// The bitmap has only the worm in the play area, so a set pixel is the worm. There
// is no time for comparing the head with every element like the other versions do.
static uint8_t hits_self(uint8_t head_x, uint8_t head_y)
{
    uint8_t x, y;
    uint8_t left = (uint8_t)(head_x - STAMP_WIDTH / 2);
    uint8_t top = (uint8_t)(head_y - STAMP_HEIGHT / 2);
    uint8_t previous_x, previous_y;

    masked = game.head & RING_MASK;
    previous_x = RING_X[masked];
    previous_y = RING_Y[masked];

    for (y = top; y < top + STAMP_HEIGHT; y++) {
        gfx_y = y;
        for (x = left; x < left + STAMP_WIDTH; x++) {
            // Most of the new stamp is on the previous one: this saves a lot of time
            if ((uint8_t)(x - previous_x + STAMP_WIDTH / 2) < STAMP_WIDTH
                && (uint8_t)(y - previous_y + STAMP_HEIGHT / 2) < STAMP_HEIGHT) {
                continue;
            }
            gfx_x = x;
            if (gfx_get() != 0 && !in_recent_stamp(x, y)) {
                return 1;
            }
        }
    }
    return 0;
}

static uint8_t rng_below(uint8_t range)
{
    return (uint8_t)(rng_next() % range);
}

static void drink_beer(void)
{
    uint8_t height;

    if (game.score != MAX_SCORE) {
        game.score++;
    }
    game.length += WORM_LENGTH_PER_BEER;
    if (game.length > WORM_MAX_LENGTH) {
        game.length = WORM_MAX_LENGTH;
    }
    if (game.drunk != 255) {
        set_drunk((uint8_t)(game.drunk + 1));
    }
    if (game.max_drunk < game.drunk) {
        game.max_drunk = game.drunk;
    }

    // The new beer is chosen first so that a tall one fits above the footer
    game.beer_index = rng_below(BEER_COUNT);
    height = beer_heights[game.beer_index];
    if (rng_below(100) < MEDKIT_PERCENT) {
        game.medkit_x = (uint8_t)(PLAY_LEFT + rng_below(PLAY_WIDTH - SPRITE_WIDTH));
        game.medkit_y = (uint8_t)(PLAY_TOP + rng_below((uint8_t)(PLAY_HEIGHT - MEDKIT_HEIGHT)));
        game.medkit_ticks_left = MEDKIT_VISIBLE_TICKS;
    }
    game.beer_x = (uint8_t)(PLAY_LEFT + rng_below(PLAY_WIDTH - SPRITE_WIDTH));
    game.beer_y = (uint8_t)(PLAY_TOP + rng_below((uint8_t)(PLAY_HEIGHT - height)));
}

static uint8_t in_item(uint8_t x, uint8_t y, uint8_t item_x, uint8_t item_y, uint8_t height)
{
    return item_x < x && item_y < y
           && x < (uint8_t)(item_x + SPRITE_WIDTH) && y < (uint8_t)(item_y + height);
}

uint8_t __fastcall__ game_tick(int8_t turn)
{
    uint8_t x, y;
    uint8_t heading;
    uint8_t events = EVENT_NONE;
    uint8_t medkit_visible = game.medkit_ticks_left != 0;

    if (turn > 0) {
        game.angle += TURN_STEP;
    } else if (turn < 0) {
        game.angle -= TURN_STEP;
    }
    heading = (uint8_t)(game.angle >> 8);
    game.pos_x += (uint16_t)vel_dx[heading];
    game.pos_y += (uint16_t)vel_dy[heading];
    x = (uint8_t)(game.pos_x >> 8);
    y = (uint8_t)(game.pos_y >> 8);

    if (x < HEAD_MIN_X || x > HEAD_MAX_X || y < HEAD_MIN_Y || y > HEAD_MAX_Y
        || hits_self(x, y)) {
        return EVENT_DEAD;
    }

    game.head++;
    masked = game.head & RING_MASK;
    RING_X[masked] = x;
    RING_Y[masked] = y;
    stamp_element(game.head);

    if (game.count < game.length) {
        game.count++;
    } else {
        // Clearing the tail also takes a bite of the next element, so that is redrawn
        stamp(game.head - game.count, 0);
        stamp_element(game.head - game.count + 1);
    }

    // Drunk wobble, -amp/2..amp/2 around the heading
    if (game.wobble_amp != 0) {
        game.angle += rng_next() % (game.wobble_amp + 1) - game.wobble_amp / 2;
    }

    if (medkit_visible) {
        game.medkit_ticks_left--;
        if (in_item(x, y, game.medkit_x, game.medkit_y, MEDKIT_HEIGHT)) {
            set_drunk(0);
            game.medkit_ticks_left = 0;
            events |= EVENT_MEDKIT;
        }
    }
    if (in_item(x, y, game.beer_x, game.beer_y, beer_heights[game.beer_index])) {
        drink_beer();
        events |= EVENT_BEER;
    }
    return events;
}
