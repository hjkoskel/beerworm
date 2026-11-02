// Kaljamato, raylib front end. No floating point in here either: images are
// scaled when loaded and drawn with raylib's integer calls, and the frame rate
// is the only clock.
#include <stdint.h>

#include "raylib.h"
#include "rlgl.h"

#include "game.h"
#include "settings.h"

// How long the game over screen ignores keys, so that the key mashing that
// ended the game does not close it right away.
#define GAME_OVER_INPUT_DELAY_FRAMES 6
#define MAX_ITEM_HEIGHT SETTINGS_MAX_SCREEN

static const unsigned char IMG_LIDL[] = {
#embed "../gfx/olut_lidl.png"
};
static const unsigned char IMG_COOP[] = {
#embed "../gfx/olut_coop.png"
};
static const unsigned char IMG_HEINEKEN[] = {
#embed "../gfx/olut_heineken.png"
};
static const unsigned char IMG_URQUELL[] = {
#embed "../gfx/olut_urquell.png"
};
static const unsigned char IMG_ALECOQ[] = {
#embed "../gfx/olut_alecoq.png"
};
static const unsigned char IMG_MEDKIT[] = {
#embed "../gfx/medkit.png"
};
static const unsigned char SND_BARNEY[] = {
#embed "../snd/barney.mp3"
};
static const unsigned char SND_DOH[] = {
#embed "../snd/doh.mp3"
};
static const unsigned char SND_WOHOO[] = {
#embed "../snd/wohoo.mp3"
};

typedef struct {
    Texture2D beers[BEER_COUNT];
    Texture2D medkit;
    Texture2D worm_stamp;
    Sound drink;
    Sound game_over;
    Sound wohoo;
} Assets;

typedef enum {
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER,
} State;

// Game is static because of the ring buffer, 256 KiB is a lot for the stack
static Game game;

// Loads a png already scaled to the given width, keeping the aspect ratio
static Texture2D load_scaled_texture(const unsigned char *data, int size, int width)
{
    Image image = LoadImageFromMemory(".png", data, size);
    int32_t height = ((int32_t)image.height * width + image.width / 2) / image.width;

    if (height < 1) {
        height = 1;
    }
    if (height > MAX_ITEM_HEIGHT) {
        height = MAX_ITEM_HEIGHT;
    }
    ImageResize(&image, width, height);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static Sound load_sound(const unsigned char *data, int size)
{
    Wave wave = LoadWaveFromMemory(".mp3", data, size);
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

// White disc that is tinted to any color when drawn. DrawLineEx would need
// float coordinates, and at the worm's step size the discs overlap into a line.
static Texture2D make_worm_stamp(uint8_t width)
{
    Image image = GenImageColor(width, width, BLANK);
    int16_t diameter = width;
    uint16_t limit = (uint16_t)(diameter * diameter);

    // Pixel centers in half pixels from the center of the disc
    for (int16_t y = 0; y < diameter; y++) {
        for (int16_t x = 0; x < diameter; x++) {
            int16_t dx = (int16_t)(2 * x + 1 - diameter);
            int16_t dy = (int16_t)(2 * y + 1 - diameter);
            if ((uint16_t)(dx * dx + dy * dy) <= limit) {
                ImageDrawPixel(&image, x, y, WHITE);
            }
        }
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static void load_assets(Assets *assets, const GameSettings *settings)
{
    int width = settings->beer_width;

    assets->beers[0] = load_scaled_texture(IMG_LIDL, (int)sizeof(IMG_LIDL), width);
    assets->beers[1] = load_scaled_texture(IMG_COOP, (int)sizeof(IMG_COOP), width);
    assets->beers[2] = load_scaled_texture(IMG_HEINEKEN, (int)sizeof(IMG_HEINEKEN), width);
    assets->beers[3] = load_scaled_texture(IMG_URQUELL, (int)sizeof(IMG_URQUELL), width);
    assets->beers[4] = load_scaled_texture(IMG_ALECOQ, (int)sizeof(IMG_ALECOQ), width);
    assets->medkit = load_scaled_texture(IMG_MEDKIT, (int)sizeof(IMG_MEDKIT), width);
    assets->worm_stamp = make_worm_stamp((uint8_t)settings->worm_width);

    assets->drink = load_sound(SND_BARNEY, (int)sizeof(SND_BARNEY));
    assets->game_over = load_sound(SND_DOH, (int)sizeof(SND_DOH));
    assets->wohoo = load_sound(SND_WOHOO, (int)sizeof(SND_WOHOO));
}

static void unload_assets(const Assets *assets)
{
    for (uint8_t i = 0; i < BEER_COUNT; i++) {
        UnloadTexture(assets->beers[i]);
    }
    UnloadTexture(assets->medkit);
    UnloadTexture(assets->worm_stamp);
    UnloadSound(assets->drink);
    UnloadSound(assets->game_over);
    UnloadSound(assets->wohoo);
}

static void vertex(int x, int y, Color color)
{
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex2i(x, y);
}

// DrawRectangleGradientEx takes a float Rectangle, this is the same quad
static void draw_background(int width, int height)
{
    rlBegin(RL_QUADS);
    vertex(0, 0, BEIGE);
    vertex(0, height, BLACK);
    vertex(width, height, YELLOW);
    vertex(width, 0, RED);
    rlEnd();
}

static void draw_worm(const Worm *worm, Texture2D stamp)
{
    int radius = worm->width / 2;
    uint8_t shade = 0;

    for (uint16_t i = 0; i < worm->count; i++) {
        Point16 p = worm_element(worm, i);
        DrawTexture(stamp, p.x - radius, p.y - radius, (Color){shade, shade, 255, 255});
        if (240 < shade) {
            shade = 0;
        }
        shade = (uint8_t)(shade + 10);
    }
}

static void draw_game(const Game *g, const Assets *assets, const GameSettings *s)
{
    int width = s->screen_width;
    int height = s->screen_height;
    uint16_t drunk = g->worm.drunk_centi;

    BeginDrawing();
    draw_background(width, height);
    DrawRectangle(g->play_area.x, g->play_area.y, g->play_area.width, g->play_area.height, BLACK);

    DrawText("Kaljamato", width / 6, 0, s->top_font_size, BLUE);
    DrawText(TextFormat("Alc: %d.%02d Score: %d", drunk / 100, drunk % 100, g->score),
             width / 3,
             height - s->bottom_margin + (s->bottom_margin - s->bottom_font_size) / 2,
             s->bottom_font_size, BEIGE);

    DrawTexture(assets->beers[g->beer_index], g->beer.x, g->beer.y, WHITE);
    if (g->medkit_frames_left != 0) {
        DrawTexture(assets->medkit, g->medkit.x, g->medkit.y, WHITE);
    }
    draw_worm(&g->worm, assets->worm_stamp);
    EndDrawing();
}

static void draw_game_over(const Game *g)
{
    uint16_t drunk = g->max_drunk_centi;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText(TextFormat("GAME OVER, max drunkness %d.%02d Score: %d", drunk / 100, drunk % 100, g->score),
             10, 10, 20, WHITE);
    EndDrawing();
}

int main(int argc, char **argv)
{
    GameSettings settings;
    if (!settings_from_cmdline(argc, argv, &settings)) {
        return 1;
    }

    InitWindow(settings.screen_width, settings.screen_height, "Kaljamato");
    SetTargetFPS(GAME_FPS);
    SetExitKey(KEY_NULL); // ESC ends the game, see below
    InitAudioDevice();

    Assets assets;
    load_assets(&assets, &settings);

    int16_t beer_heights[BEER_COUNT];
    for (uint8_t i = 0; i < BEER_COUNT; i++) {
        beer_heights[i] = (int16_t)assets.beers[i].height;
    }
    game_init(&game, &settings, beer_heights, (int16_t)assets.medkit.height);

    State state = STATE_PLAYING;
    uint8_t game_over_delay = 0;
    bool running = true;

    while (running && !WindowShouldClose()) {
        if (IsKeyReleased(KEY_F)) {
            ToggleFullscreen();
        }

        switch (state) {
        case STATE_PLAYING: {
            if (IsKeyDown(KEY_RIGHT)) {
                worm_turn(&game.worm, true);
            }
            if (IsKeyDown(KEY_LEFT)) {
                worm_turn(&game.worm, false);
            }
            if (IsKeyReleased(KEY_P)) {
                state = STATE_PAUSED;
            }
            draw_game(&game, &assets, &settings);
            if (state == STATE_PAUSED) {
                break;
            }

            uint8_t events = game_step(&game);
            if (events & GAME_EVENT_MEDKIT) {
                PlaySound(assets.wohoo);
            }
            if (events & GAME_EVENT_BEER) {
                PlaySound(assets.drink);
            }
            if ((events & GAME_EVENT_DEAD) || IsKeyPressed(KEY_ESCAPE)) {
                PlaySound(assets.game_over);
                game_over_delay = GAME_OVER_INPUT_DELAY_FRAMES;
                state = STATE_GAME_OVER;
            }
            break;
        }
        case STATE_PAUSED:
            if (IsKeyReleased(KEY_P)) {
                state = STATE_PLAYING;
            }
            draw_game(&game, &assets, &settings);
            break;
        case STATE_GAME_OVER:
            draw_game_over(&game);
            if (game_over_delay != 0) {
                game_over_delay--;
            } else if (GetKeyPressed() != 0) {
                running = false;
            }
            break;
        }
    }

    unload_assets(&assets);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
