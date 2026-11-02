// Tests for the game logic, compiled for the 6502 and run in sim65: make test
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "game.h"
#include "gfx.h"

extern uint8_t fake_bitmap[8000];

static uint16_t failures = 0;

static void check(uint8_t ok, uint16_t line)
{
    if (!ok) {
        printf("line %u FAILED\n", line);
        failures++;
    }
}
#define CHECK(cond) check((cond) ? 1 : 0, __LINE__)
#define CHECK_NEAR(a, b, tolerance) CHECK((int16_t)(a) >= (int16_t)(b) - (tolerance) && (int16_t)(a) <= (int16_t)(b) + (tolerance))

static void far_away_items(void)
{
    game.beer_x = 140;
    game.beer_y = 26;
    game.beer_index = 0;
}

static void start_game(void)
{
    memset(fake_bitmap, 0, sizeof(fake_bitmap));
    game_rng = 0xACE1;
    game_init();
    far_away_items();
}

static uint16_t pixels_set(void)
{
    uint16_t x_count = 0;
    uint16_t i;
    uint8_t b;

    for (i = 0; i < sizeof(fake_bitmap); i++) {
        b = fake_bitmap[i];
        if (b & 0xC0) x_count++;
        if (b & 0x30) x_count++;
        if (b & 0x0C) x_count++;
        if (b & 0x03) x_count++;
    }
    return x_count;
}

static void test_rng(void)
{
    uint16_t period = 0;
    uint16_t i;
    int16_t center = 0;

    game_rng = 0xACE1;
    do {
        rng_next();
        period++;
    } while (game_rng != 0xACE1 && period != 0);
    CHECK(period == 65535u);

    // A drunk worm would drift to one side if the wobble was not centered
    for (i = 0; i < 2000; i++) {
        center += (int16_t)(rng_next() % 11) - 5;
    }
    CHECK_NEAR(center / 2000, 0, 1);
}

static void test_tables(void)
{
    // 0.625 multicolor pixels and 1.25 lines per tick in 8.8
    CHECK(vel_dx[0] == 160);
    CHECK(vel_dy[0] == 0);
    CHECK(vel_dx[64] == 0);
    CHECK(vel_dy[64] == 320);
    CHECK(vel_dx[128] == -160);
    CHECK(vel_dy[192] == -320);
    CHECK(wobble_table[0] == 0);
    CHECK_NEAR(wobble_table[1], 942, 1);
}

static void test_straight_down_to_wall(void)
{
    uint16_t ticks = 0;
    uint8_t events = EVENT_NONE;

    start_game();
    CHECK(game.pos_x >> 8 == 16 && game.pos_y >> 8 == 48);
    while (events != EVENT_DEAD && ticks < 1000) {
        events = game_tick(0);
        ticks++;
    }
    CHECK(events == EVENT_DEAD);
    // From line 48 to past 181 at 1.25 lines per tick
    CHECK_NEAR(ticks, 107, 1);
    CHECK(game.pos_x >> 8 == 16);
}

static void test_turning(void)
{
    uint8_t i;
    uint8_t start_x;

    // Heading down, clockwise on screen is towards the left
    start_game();
    game.pos_x = (uint16_t)60 << 8;
    start_x = 60;
    for (i = 0; i < 10; i++) {
        CHECK(game_tick(1) == EVENT_NONE);
    }
    CHECK(game.pos_x >> 8 < start_x);

    start_game();
    game.pos_x = (uint16_t)60 << 8;
    for (i = 0; i < 10; i++) {
        CHECK(game_tick(-1) == EVENT_NONE);
    }
    CHECK(game.pos_x >> 8 > start_x);
}

static void test_circle_size_and_self_hit(void)
{
    uint8_t ticks = 0;
    uint8_t events = EVENT_NONE;
    uint8_t max_y;

    // 65536 / 1001 = 65 ticks for one round. Circling hits the worm's own start.
    start_game();
    game.pos_x = (uint16_t)80 << 8;
    game.pos_y = (uint16_t)60 << 8;
    max_y = 60;
    game.length = 300;
    while (events != EVENT_DEAD && ticks < 100) {
        events = game_tick(1);
        if ((uint8_t)(game.pos_y >> 8) > max_y) {
            max_y = (uint8_t)(game.pos_y >> 8);
        }
        ticks++;
    }
    CHECK(events == EVENT_DEAD);
    CHECK_NEAR(ticks, 60, 5);
    // Radius 1.25 / 0.096 = 13 lines
    CHECK_NEAR(max_y - 60, 13, 2);
}

static void test_short_worm_can_circle_forever(void)
{
    uint16_t i;

    // The tail is gone before the head comes around: only the start length counts
    start_game();
    game.pos_x = (uint16_t)80 << 8;
    game.pos_y = (uint16_t)60 << 8;
    game.length = 30;
    for (i = 0; i < 3000; i++) {
        CHECK(game_tick(1) == EVENT_NONE);
    }
}

static void test_crossing_wide_body_kills(void)
{
    uint8_t i;
    uint8_t events = EVENT_NONE;

    // Draws a wall of body across the path
    start_game();
    gfx_x = 40;
    gfx_y = 70;
    gfx_w = 40;
    gfx_h = 2;
    gfx_color = 2;
    gfx_rect();
    game.pos_x = (uint16_t)60 << 8;
    game.pos_y = (uint16_t)40 << 8;
    for (i = 0; i < 40 && events == EVENT_NONE; i++) {
        events = game_tick(0);
    }
    CHECK(events == EVENT_DEAD);
    CHECK_NEAR(game.pos_y >> 8, 68, 1);
}

static void test_tail_erased(void)
{
    uint16_t i;
    uint16_t after_start;
    uint16_t set;

    // Circling at fixed length, the amount of pixels on screen stays about the same
    start_game();
    game.pos_x = (uint16_t)80 << 8;
    game.pos_y = (uint16_t)60 << 8;
    game.length = 30;
    for (i = 0; i < 40; i++) {
        game_tick(1);
    }
    after_start = pixels_set();
    CHECK(after_start > 15);
    for (i = 0; i < 400; i++) {
        game_tick(1);
    }
    set = pixels_set();
    CHECK_NEAR(set, after_start, 30);
}

static void test_ring_wraps(void)
{
    uint16_t i;

    start_game();
    game.pos_x = (uint16_t)80 << 8;
    game.pos_y = (uint16_t)60 << 8;
    game.length = 30;
    for (i = 0; i < 2600; i++) {
        CHECK(game_tick(1) == EVENT_NONE);
    }
    CHECK(game.count == 30);
    // Elements stay next to each other over the wrap
    for (i = game.head - 29; i != game.head; i++) {
        CHECK_NEAR(worm_x_at(i), worm_x_at(i + 1), 1);
        CHECK_NEAR(worm_y_at(i), worm_y_at(i + 1), 2);
    }
}

static void test_beer(void)
{
    uint8_t i;
    uint8_t x = 60;

    start_game();
    game.pos_x = (uint16_t)x << 8;
    game.pos_y = (uint16_t)60 << 8;
    game.beer_x = x - 5;
    game.beer_y = 56;
    game.beer_index = 0;
    game.medkit_ticks_left = 0;
    CHECK(game_tick(0) == EVENT_BEER);
    CHECK(game.score == 1);
    CHECK(game.drunk == 1 && game.max_drunk == 1);
    CHECK(game.length == WORM_START_LENGTH + WORM_LENGTH_PER_BEER);
    CHECK(game.wobble_amp == wobble_table[1]);

    // New items stay in the play area, also with the tallest beer
    for (i = 0; i < 200; i++) {
        game.pos_x = (uint16_t)x << 8;
        game.pos_y = (uint16_t)60 << 8;
        game.angle = 0x4000;
        game.beer_x = x - 5;
        game.beer_y = 50;
        memset(fake_bitmap, 0, sizeof(fake_bitmap));
        CHECK(game_tick(0) & EVENT_BEER);
        CHECK(game.beer_x >= PLAY_LEFT && game.beer_x + SPRITE_WIDTH <= PLAY_LEFT + PLAY_WIDTH);
        CHECK(game.beer_y >= PLAY_TOP && game.beer_y + beer_heights[game.beer_index] <= PLAY_TOP + PLAY_HEIGHT);
        if (game.medkit_ticks_left != 0) {
            CHECK(game.medkit_x >= PLAY_LEFT && game.medkit_x + SPRITE_WIDTH <= PLAY_LEFT + PLAY_WIDTH);
            CHECK(game.medkit_y >= PLAY_TOP && game.medkit_y + MEDKIT_HEIGHT <= PLAY_TOP + PLAY_HEIGHT);
        }
    }
    CHECK(game.score == 201);
}

static void test_medkit(void)
{
    uint8_t x = 60;

    start_game();
    game.pos_x = (uint16_t)x << 8;
    game.pos_y = (uint16_t)40 << 8;
    game.drunk = 7;
    game.wobble_amp = 0; // no wobble so that the path is known

    // Not visible: going over it does nothing
    game.medkit_x = x - 5;
    game.medkit_y = 30;
    game.medkit_ticks_left = 0;
    CHECK(game_tick(0) == EVENT_NONE);
    CHECK(game.drunk == 7);

    // Visible: sobers up and disappears
    game.medkit_ticks_left = MEDKIT_VISIBLE_TICKS;
    CHECK(game_tick(0) == EVENT_MEDKIT);
    CHECK(game.drunk == 0 && game.wobble_amp == 0);
    CHECK(game.medkit_ticks_left == 0);

    // Times out
    game.medkit_x = 140;
    game.medkit_y = 170;
    game.medkit_ticks_left = 2;
    game_tick(0);
    CHECK(game.medkit_ticks_left == 1);
    game_tick(0);
    CHECK(game.medkit_ticks_left == 0);
    game_tick(0);
    CHECK(game.medkit_ticks_left == 0);
}

static void test_length_cap(void)
{
    start_game();
    game.length = WORM_MAX_LENGTH - 5;
    game.pos_x = (uint16_t)60 << 8;
    game.pos_y = (uint16_t)60 << 8;
    game.beer_x = 55;
    game.beer_y = 56;
    CHECK(game_tick(0) == EVENT_BEER);
    CHECK(game.length == WORM_MAX_LENGTH);
}

int main(void)
{
    test_rng();
    test_tables();
    test_straight_down_to_wall();
    test_turning();
    test_circle_size_and_self_hit();
    test_short_worm_can_circle_forever();
    test_crossing_wide_body_kills();
    test_tail_erased();
    test_ring_wraps();
    test_beer();
    test_medkit();
    test_length_cap();

    if (failures != 0) {
        printf("%u checks failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
