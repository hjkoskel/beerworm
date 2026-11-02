// gfx.h on top of a plain array, for running the game logic in sim65
#include "gfx.h"

uint8_t gfx_x;
uint8_t gfx_y;
uint8_t gfx_w;
uint8_t gfx_h;
uint8_t gfx_color;

uint8_t fake_bitmap[8000];

static uint8_t *byte_at(uint8_t x, uint8_t y)
{
    return fake_bitmap + (uint16_t)(y >> 3) * 320 + (y & 7) + (x & 0xFC) * 2;
}

static uint8_t get_at(uint8_t x, uint8_t y)
{
    return (uint8_t)((*byte_at(x, y) >> (6 - (x & 3) * 2)) & 3);
}

uint8_t gfx_get(void)
{
    return get_at(gfx_x, gfx_y);
}

void gfx_rect(void)
{
    uint8_t x, y, shift;
    uint8_t *p;

    for (y = gfx_y; y != (uint8_t)(gfx_y + gfx_h); y++) {
        for (x = gfx_x; x != (uint8_t)(gfx_x + gfx_w); x++) {
            p = byte_at(x, y);
            shift = (uint8_t)(6 - (x & 3) * 2);
            *p = (uint8_t)((*p & ~(3 << shift)) | (gfx_color << shift));
        }
    }
}
