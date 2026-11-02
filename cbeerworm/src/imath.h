// Integer math helpers.
//
// Angles are 16-bit binary angles: 65536 units are a full circle, so wrapping
// the uint16_t is wrapping the angle. Sines are Q15 (32767 is 1.0).
#ifndef IMATH_H
#define IMATH_H

#include <stdint.h>

#define ANGLE_QUARTER ((uint16_t)0x4000)

int16_t isin(uint16_t angle);
int16_t icos(uint16_t angle);

// Multiplies Q15 by any fixed-point value, keeping the format of the latter.
int16_t mul_q15(int16_t q15, int16_t value);

// (1 - e^(-drunk * factor)) in binary angle units, drunk and factor in 1/100.
uint16_t wobble_amplitude(uint16_t drunk_centi, uint16_t factor_centi);

// xorshift16 with full period. The state must not be 0.
uint16_t rng_next(uint16_t *state);
// Uniform-ish value in 0..range-1. A range of 0 gives 0.
uint16_t rng_below(uint16_t *state, uint16_t range);

uint16_t sat_add_u16(uint16_t a, uint16_t b);

#endif
