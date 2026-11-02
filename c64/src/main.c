// Kaljamato for the Commodore 64: hardware setup, drawing outside the worm, input
// and the main loop. The screen is a PAL multicolor bitmap in VIC bank 2.
#include <stdint.h>
#include <string.h>

#include "game.h"
#include "gfx.h"
#include "sound.h"

#define POKE(addr, value) (*(volatile uint8_t *)(addr) = (uint8_t)(value))
#define PEEK(addr) (*(volatile uint8_t *)(addr))

#define VIC_SPRITE_X(n) (0xD000 + (n) * 2)
#define VIC_SPRITE_Y(n) (0xD001 + (n) * 2)
#define VIC_SPRITE_X_HIGH 0xD010
#define VIC_CONTROL1 0xD011
#define VIC_RASTER 0xD012
#define VIC_SPRITE_ENABLE 0xD015
#define VIC_CONTROL2 0xD016
#define VIC_SPRITE_EXPAND_Y 0xD017
#define VIC_MEMORY 0xD018
#define VIC_SPRITE_MULTICOLOR 0xD01C
#define VIC_SPRITE_EXPAND_X 0xD01D
#define VIC_BORDER 0xD020
#define VIC_BACKGROUND 0xD021
#define VIC_SPRITE_SHARED1 0xD025
#define VIC_SPRITE_SHARED2 0xD026
#define VIC_SPRITE_COLOR(n) (0xD027 + (n))

#define CIA1_PORT_A 0xDC00
#define CIA1_PORT_B 0xDC01
#define CIA1_DDR_A 0xDC02
#define CIA2_PORT_A 0xDD00
#define CIA2_DDR_A 0xDD02
#define NMI_VECTOR 0x0318

#define SCREEN_RAM ((uint8_t *)0x8000)
#define SPRITE_POINTERS ((uint8_t *)0x83F8)
#define SPRITE_RAM ((uint8_t *)0x8400)
#define SPRITE_FIRST_POINTER 16 // (0x8400 - 0x8000) / 64
#define BITMAP ((uint8_t *)0xA000)
#define COLOR_RAM ((uint8_t *)0xD800)

#define BEER_SPRITE 0
#define MEDKIT_SPRITE 1
#define MEDKIT_FRAME BEER_COUNT
#define SPRITE_BIT(n) (1 << (n))

#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define COLOR_BLUE 6
#define COLOR_DARK_GREY 11
#define COLOR_LIGHT_BLUE 14

// Bitmap pixel values: 1 and 2 come from screen RAM, 3 from color RAM
#define TEXT_PIXEL 3
#define WORM_SCREEN_COLORS (COLOR_BLUE << 4 | COLOR_LIGHT_BLUE)

// Play area in cells, must match tools/gen_assets.py
#define PLAY_COLUMN_FIRST (PLAY_LEFT / 4)
#define PLAY_COLUMN_END ((PLAY_LEFT + PLAY_WIDTH) / 4)
#define PLAY_ROW_FIRST (PLAY_TOP / 8)
#define PLAY_ROW_END ((PLAY_TOP + PLAY_HEIGHT) / 8)
#define TITLE_COLUMN 11
#define TITLE_LINE 4
#define GAME_OVER_TOP 64
#define HUD_ROW 24
#define HUD_COLUMN 1
#define HUD_CHARACTERS 19 // "Alc:2.55 Score:9999"

#define FRAMES_PER_TICK 2

// Bayer dither by 1/4 steps between the two colors of a cell, for odd and even lines
static const uint8_t DITHER_EVEN[5] = {0x55, 0x65, 0x66, 0x66, 0xAA};
static const uint8_t DITHER_ODD[5] = {0x55, 0x55, 0x99, 0xA9, 0xAA};

extern uint8_t gfx_glyph_buf[8];
void __fastcall__ gfx_glyph(uint8_t screen_code);
void rti_stub(void);

static char text[24];

// cc65 translates strings to PETSCII, and the set in use is the lower case one
static uint8_t screen_code(char c)
{
    uint8_t p = (uint8_t)c;

    if (p >= 0x41 && p <= 0x5A) {
        return (uint8_t)(p - 0x40); // lower case letters
    }
    if (p >= 0xC1 && p <= 0xDA) {
        return (uint8_t)(p - 0x80); // upper case letters
    }
    return p;
}

// Characters are 8 multicolor pixels wide, double width compared to text mode, and
// that keeps every pixel of the font. Needs a color 0 or dithered background.
static void draw_text(uint8_t column, uint8_t top, uint8_t line_height, const char *s)
{
    uint8_t line, pixel, bits;
    uint8_t x = (uint8_t)(column * 4);

    gfx_w = 1;
    gfx_h = line_height;
    gfx_color = TEXT_PIXEL;
    for (; *s != 0; s++, x += 8) {
        gfx_glyph(screen_code(*s));
        for (line = 0; line < 8; line++) {
            bits = gfx_glyph_buf[line];
            gfx_y = (uint8_t)(top + line * line_height);
            for (pixel = 0; pixel < 8; pixel++) {
                if (bits & 0x80) {
                    gfx_x = (uint8_t)(x + pixel);
                    gfx_rect();
                }
                bits <<= 1;
            }
        }
    }
}

// Appends a decimal number and returns the new end of the text
static char *append_number(char *out, uint16_t value)
{
    char digits[5];
    uint8_t n = 0;

    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (n != 0) {
        *out++ = digits[--n];
    }
    return out;
}

static char *append_text(char *out, const char *s)
{
    while (*s != 0) {
        *out++ = *s++;
    }
    return out;
}

// Prints drunkness in 1/100 as 0.04
static char *append_alc(char *out, uint8_t centi)
{
    out = append_number(out, centi / 100);
    *out++ = '.';
    *out++ = (char)('0' + centi / 10 % 10);
    *out++ = (char)('0' + centi % 10);
    return out;
}

static void draw_hud(void)
{
    char *out = text;

    gfx_x = HUD_COLUMN * 4;
    gfx_y = HUD_ROW * 8;
    gfx_w = HUD_CHARACTERS * 8;
    gfx_h = 8;
    gfx_color = 0;
    gfx_rect();

    out = append_text(out, "Alc:");
    out = append_alc(out, game.drunk);
    out = append_text(out, " Score:");
    out = append_number(out, game.score);
    *out = 0;
    draw_text(HUD_COLUMN, HUD_ROW * 8, 1, text);
}

static void screen_off(void)
{
    POKE(VIC_CONTROL1, PEEK(VIC_CONTROL1) & 0xEF);
}

static void screen_on(void)
{
    POKE(VIC_CONTROL1, PEEK(VIC_CONTROL1) | 0x10);
}

static void draw_play_screen(void)
{
    uint8_t row, column, line;
    uint16_t cell = 0;
    uint16_t border = 0;
    uint8_t *bitmap = BITMAP;
    uint8_t level;

    memset(BITMAP, 0, 8000);
    for (row = 0; row < 25; row++) {
        for (column = 0; column < 40; column++, cell++, bitmap += 8) {
            if (row >= PLAY_ROW_FIRST && row < PLAY_ROW_END
                && column >= PLAY_COLUMN_FIRST && column < PLAY_COLUMN_END) {
                SCREEN_RAM[cell] = WORM_SCREEN_COLORS;
                COLOR_RAM[cell] = COLOR_WHITE;
                continue;
            }
            SCREEN_RAM[cell] = border_screen[border];
            COLOR_RAM[cell] = row < PLAY_ROW_FIRST ? COLOR_BLUE : COLOR_WHITE;
            level = border_levels[border];
            for (line = 0; line < 8; line += 2) {
                bitmap[line] = DITHER_EVEN[level];
                bitmap[line + 1] = DITHER_ODD[level];
            }
            border++;
        }
    }
    draw_text(TITLE_COLUMN, TITLE_LINE, 2, "Kaljamato");
    draw_hud();
}

static void draw_game_over_screen(void)
{
    char *out = text;

    POKE(VIC_SPRITE_ENABLE, 0);
    screen_off();
    memset(BITMAP, 0, 8000);
    memset(SCREEN_RAM, 0, 1000);
    memset(COLOR_RAM, COLOR_WHITE, 1000);

    draw_text(2, GAME_OVER_TOP, 2, "GAME OVER");
    out = append_text(out, "max drunkness ");
    out = append_alc(out, game.max_drunk);
    *out = 0;
    draw_text(2, GAME_OVER_TOP + 32, 1, text);
    out = append_text(text, "Score: ");
    out = append_number(out, game.score);
    *out = 0;
    draw_text(2, GAME_OVER_TOP + 48, 1, text);
    screen_on();
}

static void set_sprite_position(uint8_t sprite, uint8_t x, uint8_t y)
{
    uint16_t screen_x = (uint16_t)x * 2 + 24;
    uint8_t bit = SPRITE_BIT(sprite);

    POKE(VIC_SPRITE_X(sprite), screen_x);
    if (screen_x > 255) {
        POKE(VIC_SPRITE_X_HIGH, PEEK(VIC_SPRITE_X_HIGH) | bit);
    } else {
        POKE(VIC_SPRITE_X_HIGH, PEEK(VIC_SPRITE_X_HIGH) & (uint8_t)~bit);
    }
    POKE(VIC_SPRITE_Y(sprite), y + 50);
}

static void update_sprites(void)
{
    SPRITE_POINTERS[BEER_SPRITE] = SPRITE_FIRST_POINTER + game.beer_index;
    POKE(VIC_SPRITE_COLOR(BEER_SPRITE), beer_colors[game.beer_index]);
    set_sprite_position(BEER_SPRITE, game.beer_x, game.beer_y);

    if (game.medkit_ticks_left != 0) {
        set_sprite_position(MEDKIT_SPRITE, game.medkit_x, game.medkit_y);
        POKE(VIC_SPRITE_ENABLE, SPRITE_BIT(BEER_SPRITE) | SPRITE_BIT(MEDKIT_SPRITE));
    } else {
        POKE(VIC_SPRITE_ENABLE, SPRITE_BIT(BEER_SPRITE));
    }
}

static void init_hardware(void)
{
    __asm__("sei"); // the game does not need the KERNAL, and it would scan the keys
    // RESTORE would otherwise go through the KERNAL and break out of the game
    POKE(NMI_VECTOR, (uint16_t)rti_stub & 0xFF);
    POKE(NMI_VECTOR + 1, (uint16_t)rti_stub >> 8);

    POKE(CIA2_DDR_A, PEEK(CIA2_DDR_A) | 3);
    POKE(CIA2_PORT_A, (PEEK(CIA2_PORT_A) & 0xFC) | 1); // VIC bank 2, $8000-$BFFF
    POKE(VIC_MEMORY, 0x08);    // screen at the start of the bank, bitmap at +$2000
    POKE(VIC_CONTROL1, 0x2B);  // bitmap, screen off, 25 rows
    POKE(VIC_CONTROL2, 0x18);  // multicolor, 40 columns
    POKE(VIC_BORDER, COLOR_BLACK);
    POKE(VIC_BACKGROUND, COLOR_BLACK);

    memcpy(SPRITE_RAM, sprite_data, sizeof(sprite_data));
    SPRITE_POINTERS[MEDKIT_SPRITE] = SPRITE_FIRST_POINTER + MEDKIT_FRAME;
    POKE(VIC_SPRITE_MULTICOLOR, SPRITE_BIT(BEER_SPRITE) | SPRITE_BIT(MEDKIT_SPRITE));
    POKE(VIC_SPRITE_EXPAND_Y, SPRITE_BIT(BEER_SPRITE));
    POKE(VIC_SPRITE_EXPAND_X, 0);
    POKE(VIC_SPRITE_SHARED1, COLOR_WHITE);
    POKE(VIC_SPRITE_SHARED2, COLOR_DARK_GREY);
    POKE(VIC_SPRITE_COLOR(MEDKIT_SPRITE), MEDKIT_COLOR);

    POKE(CIA1_DDR_A, 0xFF);
    sound_init();
}

static void wait_frame(void)
{
    // Line 251 is below the picture. The tick takes longer than one line.
    while (PEEK(VIC_RASTER) != 251) {
    }
    while (PEEK(VIC_RASTER) == 251) {
    }
}

static void wait_frames(uint8_t frames)
{
    while (frames != 0) {
        wait_frame();
        frames--;
    }
}

// Keyboard matrix: a column line is driven low and the rows are read
static uint8_t key_down(uint8_t column, uint8_t row)
{
    POKE(CIA1_PORT_A, (uint8_t)~(1 << column));
    return (PEEK(CIA1_PORT_B) & (1 << row)) == 0;
}
#define KEY_Z_DOWN() key_down(1, 4)
#define KEY_X_DOWN() key_down(2, 7)
#define KEY_P_DOWN() key_down(5, 1)
#define KEY_STOP_DOWN() key_down(7, 7)

static uint8_t any_key_down(void)
{
    POKE(CIA1_PORT_A, 0);
    return PEEK(CIA1_PORT_B) != 0xFF;
}

// Joystick in port 2, active low: bit 2 left, bit 3 right, bit 4 fire
#define JOY_LEFT 0x04
#define JOY_RIGHT 0x08
#define JOY_FIRE 0x10

static uint8_t joystick(void)
{
    uint8_t value;

    POKE(CIA1_DDR_A, 0);
    value = (uint8_t)~PEEK(CIA1_PORT_A);
    POKE(CIA1_DDR_A, 0xFF);
    return value;
}

static int8_t read_turn(void)
{
    int8_t turn = 0;
    uint8_t joy = joystick();

    if ((joy & JOY_RIGHT) || KEY_X_DOWN()) {
        turn++;
    }
    if ((joy & JOY_LEFT) || KEY_Z_DOWN()) {
        turn--;
    }
    return turn;
}

static void wait_pause_ended(void)
{
    while (KEY_P_DOWN()) { // the key press that started the pause
        wait_frame();
    }
    while (!KEY_P_DOWN()) {
        wait_frame();
    }
    while (KEY_P_DOWN()) {
        wait_frame();
    }
}

static void play_game(void)
{
    uint8_t events;

    screen_off();
    draw_play_screen();
    game_init();
    update_sprites();
    screen_on();

    for (;;) {
        wait_frames(FRAMES_PER_TICK);
        if (KEY_P_DOWN()) {
            wait_pause_ended();
        }
        if (KEY_STOP_DOWN()) {
            return;
        }
        events = game_tick(read_turn());
        if (events & EVENT_DEAD) {
            return;
        }
        if (events != EVENT_NONE) {
            update_sprites();
            draw_hud();
            if (events & EVENT_MEDKIT) {
                sound_play(snd_wohoo, SND_WOHOO_BYTES);
            }
            if (events & EVENT_BEER) {
                sound_play(snd_drink, SND_DRINK_BYTES);
            }
        } else if (game.medkit_ticks_left == 0) {
            POKE(VIC_SPRITE_ENABLE, SPRITE_BIT(BEER_SPRITE));
        }
    }
}

static void wait_restart(void)
{
    while (any_key_down() || (joystick() & JOY_FIRE)) {
        wait_frame();
    }
    while (!any_key_down() && !(joystick() & JOY_FIRE)) {
        wait_frame();
    }
}

int main(void)
{
    init_hardware();
    for (;;) {
        play_game();
        draw_game_over_screen();
        sound_play(snd_game_over, SND_GAME_OVER_BYTES);
        wait_restart();
    }
    return 0;
}
