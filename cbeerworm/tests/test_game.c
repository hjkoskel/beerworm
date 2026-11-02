// Tests for the game logic, no window or raylib needed: make test
#include <stdio.h>

#include "../src/game.h"
#include "../src/imath.h"

static int failures = 0;

#define CHECK(cond)                                               \
    do {                                                          \
        if (!(cond)) {                                            \
            printf("%s:%d: FAILED %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                           \
        }                                                         \
    } while (0)

#define CHECK_NEAR(a, b, tolerance) CHECK((a) >= (b) - (tolerance) && (a) <= (b) + (tolerance))

static Game game; // big because of the ring buffer

static void start_game(void)
{
    GameSettings s = settings_default();
    const int16_t heights[BEER_COUNT] = {110, 109, 110, 157, 111};
    game_init(&game, &s, heights, 53);
}

static void test_sine(void)
{
    CHECK(isin(0) == 0);
    CHECK(isin(0x4000) == 32767);
    CHECK(isin(0x8000) == 0);
    CHECK(isin(0xC000) == -32767);
    CHECK(icos(0) == 32767);
    CHECK(icos(0x8000) == -32767);
    // 45 degrees and 30 degrees, between the table entries
    CHECK_NEAR(isin(0x2000), 23170, 1);
    CHECK_NEAR(isin(5461), 16383, 4);
    CHECK(isin(1000) == -isin((uint16_t)(0 - 1000)));
    CHECK(isin(1000) == isin((uint16_t)(0x8000 - 1000)));
}

static void test_wobble(void)
{
    CHECK(wobble_amplitude(0, 600) == 0);
    // (1 - e^-0.06) * 10430 is 607
    CHECK_NEAR(wobble_amplitude(1, 600), 607, 2);
    // (1 - e^-3) * 10430 is 9911
    CHECK_NEAR(wobble_amplitude(50, 600), 9911, 2);
    CHECK(wobble_amplitude(UINT16_MAX, UINT16_MAX) == 10427);
}

static void test_rng(void)
{
    uint16_t state = 0xACE1;
    uint32_t period = 0;
    do {
        rng_next(&state);
        period++;
    } while (state != 0xACE1 && period <= 65536);
    CHECK(period == 65535);

    // A long wobble has to stay centered or a drunk worm drifts to one side
    int32_t sum = 0;
    for (uint32_t i = 0; i < 65535; i++) {
        sum += (int32_t)rng_below(&state, 1001) - 500;
    }
    CHECK_NEAR(sum / 65535, 0, 3);
}

static void test_settings_parsing(void)
{
    uint16_t v = 0;
    CHECK(settings_parse_decimal("1.85", 2, &v) && v == 185);
    CHECK(settings_parse_decimal("0.04", 4, &v) && v == 400);
    CHECK(settings_parse_decimal(".3", 2, &v) && v == 30);
    CHECK(settings_parse_decimal("17", 0, &v) && v == 17);
    CHECK(settings_parse_decimal("17.9", 0, &v) && v == 17);
    CHECK(settings_parse_decimal("1.8567", 2, &v) && v == 185);
    CHECK(!settings_parse_decimal("", 0, &v));
    CHECK(!settings_parse_decimal(".", 2, &v));
    CHECK(!settings_parse_decimal("1.2.3", 2, &v));
    CHECK(!settings_parse_decimal("-1", 0, &v));
    CHECK(!settings_parse_decimal("x", 0, &v));
    CHECK(!settings_parse_decimal("65536", 0, &v));
    CHECK(!settings_parse_decimal("700", 2, &v));

    GameSettings s;
    char *ok[] = {"kaljamato", "-w=800", "-ws=2.5", "-wt=0.05", "-kp=0.5", "-kt=1000", "-ww=9", "-unknown=1"};
    CHECK(settings_from_cmdline(8, ok, &s));
    CHECK(s.screen_width == 800 && s.worm_speed_centi == 250 && s.worm_turn_decimilli == 500);
    CHECK(s.medkit_percent == 50 && s.medkit_visible_ms == 1000 && s.worm_width == 9);
    CHECK(s.screen_height == 768);

    char *bad_value[] = {"kaljamato", "-w=abc"};
    CHECK(!settings_from_cmdline(2, bad_value, &s));
    char *too_wide[] = {"kaljamato", "-ww=181"};
    CHECK(!settings_from_cmdline(2, too_wide, &s));
    char *no_area[] = {"kaljamato", "-ms=512"};
    CHECK(!settings_from_cmdline(2, no_area, &s));
}

static void test_conversions(void)
{
    start_game();
    CHECK(game.worm.turn_step == 417);   // 0.04 rad
    CHECK(game.worm.speed_q8 == 474);    // 1.85 pixels
    CHECK(game.medkit_visible_frames == 480);
    CHECK(game.play_area.x == 50 && game.play_area.y == 100);
    CHECK(game.play_area.width == 924 && game.play_area.height == 618);
    CHECK(game.beer.x == 512 && game.beer.y == 384);
}

static void test_straight_line_keeps_subpixels(void)
{
    start_game();
    Worm *w = &game.worm;
    Point16 start = w->body[w->head_index];

    // Heading 0.5 radians off the axes: x and y have different fractions
    w->angle = (uint16_t)5215;
    for (uint16_t i = 0; i < 1000; i++) {
        worm_move(w, &game.rng);
    }
    Point16 end = w->body[w->head_index];
    // 1000 * 1.8516 * cos(0.5) and sin(0.5). The velocity has 1/256 pixel
    // steps, so 1000 frames can be off by 2 pixels but the fraction is not lost
    // (without sub-pixels this would be 2000, 1000 or some such).
    CHECK_NEAR(end.x - start.x, 1625, 2);
    CHECK_NEAR(end.y - start.y, 888, 2);
    CHECK(w->count == WORM_START_LENGTH);
}

static void test_full_circle(void)
{
    start_game();
    Worm *w = &game.worm;
    Point16 start = w->body[w->head_index];

    // 65536 / 417 frames makes one round, and the radius is about 46 pixels
    int16_t max_y = start.y;
    for (uint16_t i = 0; i < 157; i++) {
        worm_turn(w, true);
        worm_move(w, &game.rng);
        if (w->body[w->head_index].y > max_y) {
            max_y = w->body[w->head_index].y;
        }
    }
    Point16 end = w->body[w->head_index];
    CHECK_NEAR(end.x, start.x, 3);
    CHECK_NEAR(end.y, start.y, 3);
    CHECK_NEAR(max_y - start.y, 46, 2);
}

static void test_turn_directions(void)
{
    start_game();
    Worm *w = &game.worm;
    int16_t start_x = w->body[w->head_index].x;

    // Heading down: clockwise on screen goes towards -x, like in the zig version
    for (uint16_t i = 0; i < 20; i++) {
        worm_turn(w, true);
        worm_move(w, &game.rng);
    }
    CHECK(w->body[w->head_index].x < start_x);
}

static void test_self_collision(void)
{
    start_game();
    Worm *w = &game.worm;

    // Going around the 46 pixel circle and hitting the start of the worm
    w->length = 400;
    bool hit = false;
    for (uint16_t i = 0; i < 200 && !hit; i++) {
        worm_turn(w, true);
        worm_move(w, &game.rng);
        hit = worm_hit_self(w);
    }
    CHECK(hit);

    // The elements right behind the head do not count
    start_game();
    w->length = 400;
    for (uint16_t i = 0; i < 300; i++) {
        worm_move(w, &game.rng);
        CHECK(!worm_hit_self(w));
    }
}

static void test_ring_buffer_wraps(void)
{
    start_game();
    Worm *w = &game.worm;
    w->turn_step = 417;

    for (uint32_t i = 0; i < 70000; i++) {
        worm_turn(w, true);
        worm_move(w, &game.rng);
    }
    CHECK(w->count == WORM_START_LENGTH);
    Point16 head = worm_element(w, (uint16_t)(w->count - 1));
    CHECK(head.x == w->body[w->head_index].x && head.y == w->body[w->head_index].y);
    for (uint16_t i = 1; i < w->count; i++) {
        Point16 a = worm_element(w, (uint16_t)(i - 1));
        Point16 b = worm_element(w, i);
        CHECK_NEAR(a.x, b.x, 2);
        CHECK_NEAR(a.y, b.y, 2);
    }
}

static void test_length_saturates_at_capacity(void)
{
    start_game();
    Worm *w = &game.worm;
    w->length = UINT16_MAX;
    w->count = UINT16_MAX;
    worm_move(w, &game.rng);
    CHECK(w->count == UINT16_MAX);
    CHECK(sat_add_u16(UINT16_MAX, 80) == UINT16_MAX);
    CHECK(sat_add_u16(65000, 500) == 65500);
}

static void test_beer(void)
{
    start_game();
    Worm *w = &game.worm;
    Point16 head = w->body[w->head_index];

    // Beer right under the head
    game.beer = (Point16){(int16_t)(head.x - 30), (int16_t)(head.y - 10)};
    Point16 old_beer = game.beer;
    uint8_t events = game_step(&game);
    CHECK(events == GAME_EVENT_BEER);
    CHECK(game.score == 1);
    CHECK(w->length == WORM_START_LENGTH + 80);
    CHECK(w->drunk_centi == 1 && game.max_drunk_centi == 1);
    CHECK(w->wobble_amp != 0);
    CHECK(game.beer.x != old_beer.x || game.beer.y != old_beer.y);
}

static void test_items_spawn_inside_play_area(void)
{
    start_game();
    game.medkit_percent = 100;
    for (uint16_t i = 0; i < 5000; i++) {
        // Keeps the head in place and a beer on it, so every step drinks one
        Worm *w = &game.worm;
        w->body[w->head_index] = (Point16){100, 200};
        w->angle = 0;
        game.beer = (Point16){80, 190};
        int16_t max_y = (int16_t)(100 + 618 - game.beer_heights[game.beer_index]);

        CHECK(game_step(&game) & GAME_EVENT_BEER);
        CHECK(game.beer.x >= 50 && game.beer.x < 50 + 924 - 60);
        CHECK(game.beer.y >= 100 && game.beer.y < max_y);
        CHECK(game.medkit.x >= 50 && game.medkit.x < 50 + 924 - 60);
        CHECK(game.medkit.y >= 100 && game.medkit.y < max_y);
        CHECK(game.medkit_frames_left == game.medkit_visible_frames);
    }
}

static void test_medkit(void)
{
    start_game();
    Worm *w = &game.worm;
    Point16 head = w->body[w->head_index];
    worm_set_drunk(w, 7);
    game.beer = (Point16){900, 600};

    // Not visible: passing over it does nothing
    game.medkit = (Point16){(int16_t)(head.x - 30), (int16_t)(head.y - 10)};
    game.medkit_frames_left = 0;
    CHECK(game_step(&game) == GAME_EVENT_NONE);
    CHECK(w->drunk_centi == 7);

    // Visible: sobers up and disappears
    game.medkit_frames_left = 480;
    CHECK(game_step(&game) == GAME_EVENT_MEDKIT);
    CHECK(w->drunk_centi == 0 && w->wobble_amp == 0);
    CHECK(game.medkit_frames_left == 0);

    // Times out by frames
    game.medkit = (Point16){900, 150};
    game.medkit_frames_left = 2;
    game_step(&game);
    CHECK(game.medkit_frames_left == 1);
    game_step(&game);
    CHECK(game.medkit_frames_left == 0);
    game_step(&game);
    CHECK(game.medkit_frames_left == 0);
}

static void test_leaving_play_area_kills(void)
{
    start_game();
    game.beer = (Point16){900, 150};
    uint16_t frames = 0;
    uint8_t events = GAME_EVENT_NONE;
    while (!(events & GAME_EVENT_DEAD) && frames < 1000) {
        events = game_step(&game);
        frames++;
    }
    CHECK(events & GAME_EVENT_DEAD);
    // From y=200 to the bottom of the play area at y=718, at 1.85 pixels per frame
    CHECK_NEAR(frames, 280, 2);
}

int main(void)
{
    test_sine();
    test_wobble();
    test_rng();
    test_settings_parsing();
    test_conversions();
    test_straight_line_keeps_subpixels();
    test_full_circle();
    test_turn_directions();
    test_self_collision();
    test_ring_buffer_wraps();
    test_length_saturates_at_capacity();
    test_beer();
    test_items_spawn_inside_play_area();
    test_medkit();
    test_leaving_play_area_kills();

    if (failures != 0) {
        printf("%d checks failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
