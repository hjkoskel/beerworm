; The SID master volume register is used as a 4-bit DAC. The voices hold a steady
; maximum output (test bit on, gate on, full sustain) for the volume to scale.
; The 6581 would not need it, but it is what makes the sound audible on an 8580.
; Silence is volume 0.
        .import popax
        .importzp ptr1, ptr2
        .export _sound_init, _sound_play

SID = $D400
SID_VOICE_STEP = 7
SID_FILTER_ROUTE = $D417
SID_VOLUME = $D418
WAVE_TEST_PULSE_GATE = $49

CIA2_TIMER_A = $DD04
CIA2_ICR = $DD0D
CIA2_CRA = $DD0E
CRA_FORCE_LOAD_START = $11

CYCLES_PER_SAMPLE = 123         ; PAL, 985248 / 123 = 8010 Hz

        .code
_sound_init:
        ldx     #0
        ldy     #3
@voice: lda     #0
        sta     SID+0,x         ; frequency
        sta     SID+1,x
        sta     SID+2,x         ; pulse width
        sta     SID+3,x
        sta     SID+5,x         ; fastest attack and decay
        lda     #$F0
        sta     SID+6,x         ; full sustain
        lda     #WAVE_TEST_PULSE_GATE
        sta     SID+4,x
        txa
        clc
        adc     #SID_VOICE_STEP
        tax
        dey
        bne     @voice
        lda     #0
        sta     SID_FILTER_ROUTE
        sta     SID_VOLUME
        lda     #$7F
        sta     CIA2_ICR        ; no NMIs, the timer flag is polled
        rts

; void sound_play(const uint8_t *data, uint16_t bytes)
_sound_play:
        sta     ptr2            ; bytes
        stx     ptr2+1
        jsr     popax
        sta     ptr1            ; data
        stx     ptr1+1

        lda     #<(CYCLES_PER_SAMPLE - 1)
        sta     CIA2_TIMER_A
        lda     #>(CYCLES_PER_SAMPLE - 1)
        sta     CIA2_TIMER_A+1
        lda     CIA2_ICR        ; clears an old flag
        lda     #CRA_FORCE_LOAD_START
        sta     CIA2_CRA

        ldy     #0
@next:  lda     (ptr1),y
        tax
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        jsr     wait_timer
        sta     SID_VOLUME
        txa
        and     #$0F
        jsr     wait_timer
        sta     SID_VOLUME

        iny
        bne     @same_page
        inc     ptr1+1
@same_page:
        lda     ptr2
        bne     @low
        dec     ptr2+1
@low:   dec     ptr2
        lda     ptr2
        ora     ptr2+1
        bne     @next

        lda     #0
        sta     CIA2_CRA
        sta     SID_VOLUME
        rts

; Keeps A. Waits for the timer, so the rate holds even when the VIC steals cycles.
wait_timer:
        pha
        lda     #1
@wait:  bit     CIA2_ICR
        beq     @wait
        pla
        rts
