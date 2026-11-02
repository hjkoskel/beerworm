// Multicolor bitmap drawing, 160 x 200 pixels with the colors 0-3.
// Parameters are globals, it is faster than the cc65 stack and there is one caller
// at a time. Implemented in gfx.s, and in tests/fake_gfx.c for the tests.
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

extern uint8_t gfx_x;
extern uint8_t gfx_y;
extern uint8_t gfx_w;
extern uint8_t gfx_h;
extern uint8_t gfx_color;

// Fills gfx_w x gfx_h pixels at gfx_x, gfx_y with gfx_color
void gfx_rect(void);
// Color of the pixel at gfx_x, gfx_y
uint8_t gfx_get(void);

#endif
