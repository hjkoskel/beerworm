// 4-bit digitized sound through the SID volume register.
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

void sound_init(void);
// Plays two 4-bit samples per byte, high nibble first, at 8 kHz. Blocks until done.
void __fastcall__ sound_play(const uint8_t *data, uint16_t bytes);

#endif
