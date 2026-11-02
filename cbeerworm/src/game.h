// Game logic without raylib, so it can be tested without a window.
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

#define BEER_COUNT 5
#define GAME_FPS 60
#define WORM_START_LENGTH 100
// body[] is indexed by a wrapping uint16_t, which makes it a ring buffer
#define WORM_CAPACITY 65536

typedef struct {
    int16_t x;
    int16_t y;
} Point16;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} Rect16;

typedef struct {
    Point16 body[WORM_CAPACITY];
    uint16_t head_index;
    uint16_t count;  // elements in use, never above length
    uint16_t length; // elements the worm is growing towards

    uint16_t angle;     // heading, 0 is +x and growing turns clockwise on screen
    uint16_t turn_step; // binary angle units per frame
    int16_t speed_q8;   // pixels per frame
    uint8_t frac_x;     // sub-pixel part of the head position, Q8
    uint8_t frac_y;
    uint8_t width;

    uint16_t drunk_centi; // shown as "Alc", 1 beer is 1/100
    uint16_t drunk_factor_centi;
    uint16_t wobble_amp; // cached from drunk_centi, binary angle units
} Worm;

typedef struct {
    Worm worm;
    uint16_t rng;

    Rect16 play_area;
    uint16_t length_inc_per_beer;
    int16_t item_width;
    int16_t beer_heights[BEER_COUNT];
    int16_t medkit_height;

    Point16 beer;
    uint8_t beer_index;

    Point16 medkit;
    uint16_t medkit_frames_left; // visible when non zero
    uint16_t medkit_visible_frames;
    uint8_t medkit_percent;

    uint16_t score;
    uint16_t max_drunk_centi;
} Game;

typedef enum {
    GAME_EVENT_NONE = 0,
    GAME_EVENT_BEER = 1 << 0,
    GAME_EVENT_MEDKIT = 1 << 1,
    GAME_EVENT_DEAD = 1 << 2,
} GameEvent;

void game_init(Game *game, const GameSettings *settings,
               const int16_t beer_heights[BEER_COUNT], int16_t medkit_height);

// Advances one frame and returns a mask of GameEvent.
uint8_t game_step(Game *game);

void worm_init(Worm *worm, const GameSettings *settings);
void worm_turn(Worm *worm, bool clockwise);
void worm_move(Worm *worm, uint16_t *rng);
void worm_set_drunk(Worm *worm, uint16_t drunk_centi);
bool worm_hit_self(const Worm *worm);
bool worm_head_in_area(const Worm *worm, Rect16 area);

// Element counted from the tail: 0 is the tail, count - 1 is the head.
Point16 worm_element(const Worm *worm, uint16_t from_tail);

#endif
