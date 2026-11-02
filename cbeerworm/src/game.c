#include "game.h"

#include "imath.h"

#define RNG_SEED 0xACE1 // fixed like in the other versions: every game is the same
#define BEER_DRUNK_INC_CENTI 1

// 65536 / (2 * pi * 10000) binary angle units per 1/10000 radian
static uint16_t turn_step_from_decimilli(uint16_t decimilli)
{
    return (uint16_t)(((uint32_t)decimilli * 65536 + 31416) / 62832);
}

static int16_t speed_q8_from_centi(uint16_t centi)
{
    return (int16_t)(((uint32_t)centi * 256 + 50) / 100);
}

static uint16_t frames_from_ms(uint16_t ms)
{
    return (uint16_t)((uint32_t)ms * GAME_FPS / 1000);
}

void worm_init(Worm *worm, const GameSettings *settings)
{
    Point16 start = {(int16_t)(settings->side_margin * 2), (int16_t)(settings->top_margin * 2)};

    worm->body[0] = start;
    worm->body[1] = start;
    worm->head_index = 1;
    worm->count = 2;
    worm->length = WORM_START_LENGTH;

    worm->angle = ANGLE_QUARTER; // downwards
    worm->turn_step = turn_step_from_decimilli(settings->worm_turn_decimilli);
    worm->speed_q8 = speed_q8_from_centi(settings->worm_speed_centi);
    worm->frac_x = 0;
    worm->frac_y = 0;
    worm->width = (uint8_t)settings->worm_width;

    worm->drunk_factor_centi = settings->drunk_factor_centi;
    worm_set_drunk(worm, 0);
}

void worm_set_drunk(Worm *worm, uint16_t drunk_centi)
{
    worm->drunk_centi = drunk_centi;
    worm->wobble_amp = wobble_amplitude(drunk_centi, worm->drunk_factor_centi);
}

void worm_turn(Worm *worm, bool clockwise)
{
    worm->angle = clockwise ? (uint16_t)(worm->angle + worm->turn_step)
                            : (uint16_t)(worm->angle - worm->turn_step);
}

// Adds a Q8 step to a position. speed_q8 is at most 100 pixels, so the sum fits.
static int16_t advance(int16_t position, uint8_t *frac, int16_t step_q8)
{
    int16_t sum = (int16_t)(*frac + step_q8);
    *frac = (uint8_t)(sum & 0xFF);
    return (int16_t)(position + (sum >> 8));
}

void worm_move(Worm *worm, uint16_t *rng)
{
    Point16 head = worm->body[worm->head_index];

    head.x = advance(head.x, &worm->frac_x, mul_q15(icos(worm->angle), worm->speed_q8));
    head.y = advance(head.y, &worm->frac_y, mul_q15(isin(worm->angle), worm->speed_q8));

    worm->head_index++;
    worm->body[worm->head_index] = head;
    if (worm->count < worm->length) {
        worm->count++;
    }

    // Drunk wobble, -amp/2..amp/2 around the heading
    if (worm->wobble_amp != 0) {
        uint16_t wobble = rng_below(rng, (uint16_t)(worm->wobble_amp + 1));
        worm->angle = (uint16_t)(worm->angle + wobble - worm->wobble_amp / 2);
    }
}

Point16 worm_element(const Worm *worm, uint16_t from_tail)
{
    uint16_t tail = (uint16_t)(worm->head_index - worm->count + 1);
    return worm->body[(uint16_t)(tail + from_tail)];
}

static bool point_in_rect(Point16 p, Rect16 r)
{
    return r.x < p.x && r.y < p.y && p.x < r.x + r.width && p.y < r.y + r.height;
}

bool worm_head_in_area(const Worm *worm, Rect16 area)
{
    return point_in_rect(worm->body[worm->head_index], area);
}

static uint16_t abs_diff(int16_t a, int16_t b)
{
    return (a < b) ? (uint16_t)(b - a) : (uint16_t)(a - b);
}

bool worm_hit_self(const Worm *worm)
{
    Point16 head = worm->body[worm->head_index];
    uint16_t width = worm->width;
    uint16_t min_distance_sq = (uint16_t)(width * width);
    bool still_in_head = true; // the elements right behind the head are always close

    // Starts at the head and leaves out the tail element, like the zig version
    uint16_t index = worm->head_index;
    for (uint16_t i = 1; i < worm->count; i++, index--) {
        Point16 elem = worm->body[index];
        uint16_t dx = abs_diff(elem.x, head.x);
        uint16_t dy = abs_diff(elem.y, head.y);

        // Squares of values below width fit in 16 bits, see SETTINGS_MAX_WORM_WIDTH
        bool near = dx < width && dy < width
                    && (uint16_t)(dx * dx + dy * dy) < min_distance_sq;
        if (near) {
            if (!still_in_head) {
                return true;
            }
        } else {
            still_in_head = false;
        }
    }
    return false;
}

static Point16 random_point_in(uint16_t *rng, Rect16 area)
{
    // The width and height are negative if the item is bigger than the play area
    uint16_t range_x = area.width > 0 ? (uint16_t)area.width : 0;
    uint16_t range_y = area.height > 0 ? (uint16_t)area.height : 0;
    int16_t x = (int16_t)(area.x + rng_below(rng, range_x));
    int16_t y = (int16_t)(area.y + rng_below(rng, range_y));
    return (Point16){x, y};
}

void game_init(Game *game, const GameSettings *settings,
               const int16_t beer_heights[BEER_COUNT], int16_t medkit_height)
{
    worm_init(&game->worm, settings);
    game->rng = RNG_SEED;

    game->play_area = (Rect16){
        .x = (int16_t)settings->side_margin,
        .y = (int16_t)settings->top_margin,
        .width = (int16_t)(settings->screen_width - settings->side_margin * 2),
        .height = (int16_t)(settings->screen_height - settings->bottom_margin - settings->top_margin),
    };
    game->length_inc_per_beer = settings->length_inc_per_beer;
    game->item_width = (int16_t)settings->beer_width;
    for (uint8_t i = 0; i < BEER_COUNT; i++) {
        game->beer_heights[i] = beer_heights[i];
    }
    game->medkit_height = medkit_height;

    Point16 center = {(int16_t)(settings->screen_width / 2), (int16_t)(settings->screen_height / 2)};
    game->beer = center;
    game->beer_index = (uint8_t)rng_below(&game->rng, BEER_COUNT);

    game->medkit = center;
    game->medkit_frames_left = 0;
    game->medkit_visible_frames = frames_from_ms(settings->medkit_visible_ms);
    game->medkit_percent = (uint8_t)settings->medkit_percent;

    game->score = 0;
    game->max_drunk_centi = 0;
}

static void drink_beer(Game *game)
{
    Worm *worm = &game->worm;
    int16_t beer_height = game->beer_heights[game->beer_index];

    game->score = sat_add_u16(game->score, 1);
    worm->length = sat_add_u16(worm->length, game->length_inc_per_beer);
    worm_set_drunk(worm, sat_add_u16(worm->drunk_centi, BEER_DRUNK_INC_CENTI));
    if (game->max_drunk_centi < worm->drunk_centi) {
        game->max_drunk_centi = worm->drunk_centi;
    }

    Rect16 spawn_area = {
        .x = game->play_area.x,
        .y = game->play_area.y,
        .width = (int16_t)(game->play_area.width - game->item_width),
        .height = (int16_t)(game->play_area.height - beer_height),
    };
    if (rng_below(&game->rng, 100) < game->medkit_percent) {
        game->medkit = random_point_in(&game->rng, spawn_area);
        game->medkit_frames_left = game->medkit_visible_frames;
    }
    game->beer = random_point_in(&game->rng, spawn_area);
    game->beer_index = (uint8_t)rng_below(&game->rng, BEER_COUNT);
}

uint8_t game_step(Game *game)
{
    Worm *worm = &game->worm;
    uint8_t events = GAME_EVENT_NONE;
    bool medkit_visible = game->medkit_frames_left != 0;

    if (medkit_visible) {
        game->medkit_frames_left--;
    }

    worm_move(worm, &game->rng);

    Rect16 medkit_rect = {game->medkit.x, game->medkit.y, game->item_width, game->medkit_height};
    if (medkit_visible && worm_head_in_area(worm, medkit_rect)) {
        worm_set_drunk(worm, 0);
        game->medkit_frames_left = 0;
        events |= GAME_EVENT_MEDKIT;
    }

    Rect16 beer_rect = {game->beer.x, game->beer.y, game->item_width,
                        game->beer_heights[game->beer_index]};
    if (worm_head_in_area(worm, beer_rect)) {
        drink_beer(game);
        events |= GAME_EVENT_BEER;
    }

    if (worm_hit_self(worm) || !worm_head_in_area(worm, game->play_area)) {
        events |= GAME_EVENT_DEAD;
    }
    return events;
}
