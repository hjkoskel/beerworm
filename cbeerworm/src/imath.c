#include "imath.h"

// round(sin(i * pi / 128) * 32767), the first quarter of the wave
static const int16_t SIN_QUARTER[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179,
    7962, 8739, 9512, 10278, 11039, 11793, 12539, 13279, 14010, 14732,
    15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403,
    22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571,
    30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521,
    32609, 32678, 32728, 32757, 32767,
};

// round((1 - e^(-i / 16)) * 65536 / (2 * pi))
// The zig version multiplies a random -0.5..0.5 radians by 1 - e^-x. Here the
// radians are binary angle units already, 10430 of them in one radian.
#define WOBBLE_STEPS 128
#define WOBBLE_X_PER_STEP 625 // x is in 1/10000, table steps are 1/16
static const uint16_t WOBBLE[WOBBLE_STEPS + 1] = {
    0, 632, 1226, 1783, 2307, 2799, 3262, 3696, 4104, 4487,
    4847, 5186, 5503, 5802, 6082, 6346, 6593, 6826, 7044, 7249,
    7442, 7623, 7793, 7953, 8103, 8244, 8377, 8501, 8618, 8728,
    8831, 8928, 9019, 9104, 9185, 9260, 9331, 9398, 9460, 9519,
    9574, 9626, 9675, 9721, 9764, 9804, 9842, 9878, 9911, 9943,
    9972, 10000, 10026, 10050, 10073, 10095, 10115, 10134, 10152, 10169,
    10185, 10200, 10214, 10227, 10239, 10251, 10262, 10272, 10282, 10291,
    10299, 10307, 10315, 10322, 10328, 10334, 10340, 10346, 10351, 10356,
    10360, 10364, 10368, 10372, 10376, 10379, 10382, 10385, 10388, 10390,
    10393, 10395, 10397, 10399, 10401, 10403, 10405, 10406, 10408, 10409,
    10410, 10411, 10413, 10414, 10415, 10416, 10417, 10417, 10418, 10419,
    10420, 10420, 10421, 10421, 10422, 10422, 10423, 10423, 10424, 10424,
    10425, 10425, 10425, 10426, 10426, 10426, 10426, 10427, 10427,
};

int16_t isin(uint16_t angle)
{
    uint16_t in_quarter = angle & (ANGLE_QUARTER - 1);
    uint8_t quadrant = (uint8_t)(angle >> 14);

    if (quadrant & 1) {
        in_quarter = (uint16_t)(ANGLE_QUARTER - in_quarter); // mirror, can be 0x4000
    }
    uint8_t index = (uint8_t)(in_quarter >> 8);
    uint8_t frac = (uint8_t)(in_quarter & 0xFF);

    int16_t value = SIN_QUARTER[index];
    if (frac != 0) {
        int32_t span = SIN_QUARTER[index + 1] - value;
        value = (int16_t)(value + ((span * frac) >> 8));
    }
    return (quadrant & 2) ? (int16_t)-value : value;
}

int16_t icos(uint16_t angle)
{
    return isin((uint16_t)(angle + ANGLE_QUARTER));
}

int16_t mul_q15(int16_t q15, int16_t value)
{
    return (int16_t)(((int32_t)q15 * value + (1 << 14)) >> 15);
}

uint16_t wobble_amplitude(uint16_t drunk_centi, uint16_t factor_centi)
{
    uint32_t x = (uint32_t)drunk_centi * factor_centi;
    uint32_t index = x / WOBBLE_X_PER_STEP;

    if (index >= WOBBLE_STEPS) {
        return WOBBLE[WOBBLE_STEPS];
    }
    uint32_t frac = x % WOBBLE_X_PER_STEP;
    uint32_t span = (uint32_t)(WOBBLE[index + 1] - WOBBLE[index]);
    return (uint16_t)(WOBBLE[index] + span * frac / WOBBLE_X_PER_STEP);
}

uint16_t rng_next(uint16_t *state)
{
    uint16_t x = *state;
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    *state = x;
    return x;
}

uint16_t rng_below(uint16_t *state, uint16_t range)
{
    return (range == 0) ? 0 : (uint16_t)(rng_next(state) % range);
}

uint16_t sat_add_u16(uint16_t a, uint16_t b)
{
    return (b > (uint16_t)(UINT16_MAX - a)) ? UINT16_MAX : (uint16_t)(a + b);
}
