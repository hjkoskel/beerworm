// Game logic. All drawing goes through gfx.h, so this also runs in the tests.
// Everything is in multicolor pixels (x 0-159) and raster lines (y 0-199).
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#include "assets.h"

#define TICKS_PER_SECOND 25 // the game runs on every second PAL frame

#define PLAY_LEFT 8
#define PLAY_TOP 24
#define PLAY_WIDTH 144
#define PLAY_HEIGHT 160

#define WORM_START_LENGTH 42  // 1.67 s of movement, the 100 frames of the original
#define WORM_LENGTH_PER_BEER 33 // 1.33 s, the original's 80 frames
#define WORM_MAX_LENGTH 2047  // ring buffer size - 1

#define STAMP_WIDTH 3 // size of the worm, about 6 x 5 on screen
#define STAMP_HEIGHT 5

#define MAX_SCORE 9999 // what fits on the footer

#define MEDKIT_PERCENT 30
#define MEDKIT_VISIBLE_TICKS (8 * TICKS_PER_SECOND)

#define EVENT_NONE 0
#define EVENT_BEER 1
#define EVENT_MEDKIT 2
#define EVENT_DEAD 4

typedef struct {
    uint16_t pos_x;     // 8.8 fixed point
    uint16_t pos_y;
    uint16_t angle;     // 65536 is a full circle, 0 is +x, growing is clockwise
    uint16_t head;      // ring index of the head
    uint16_t count;     // elements in use
    uint16_t length;    // elements the worm is growing towards
    uint16_t wobble_amp;
    uint8_t drunk;      // in 1/100, shown as "Alc", one per beer

    uint8_t beer_x;
    uint8_t beer_y;
    uint8_t beer_index;

    uint8_t medkit_x;
    uint8_t medkit_y;
    uint8_t medkit_ticks_left; // visible when not 0

    uint16_t score;
    uint8_t max_drunk;
} Game;

extern Game game;
extern uint16_t game_rng;

// Resets everything but the random numbers, and draws the start of the worm
void game_init(void);

// One step: turn is -1, 0 or 1 (clockwise). Returns a mask of EVENT_*.
uint8_t __fastcall__ game_tick(int8_t turn);

uint16_t rng_next(void);

// Exposed for the tests
uint8_t __fastcall__ worm_x_at(uint16_t ring_index);
uint8_t __fastcall__ worm_y_at(uint16_t ring_index);

#endif
