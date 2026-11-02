; Multicolor bitmap routines, see gfx.h. The bitmap is at $A000 under the BASIC
; ROM, which the cc65 startup code has switched off.
        .importzp ptr1, ptr2, tmp1, tmp2, tmp3, tmp4
        .export _gfx_x, _gfx_y, _gfx_w, _gfx_h, _gfx_color
        .export _gfx_rect, _gfx_get, _gfx_glyph, _gfx_glyph_buf

BITMAP = $A000
CHARGEN_LOWERCASE = $D800
PORT = $01

        .bss
_gfx_x:         .res 1
_gfx_y:         .res 1
_gfx_w:         .res 1
_gfx_h:         .res 1
_gfx_color:     .res 1
_gfx_glyph_buf: .res 8

        .rodata
row_lo:
        .repeat 200, i
        .byte <(BITMAP + (i >> 3) * 320 + (i & 7))
        .endrepeat
row_hi:
        .repeat 200, i
        .byte >(BITMAP + (i >> 3) * 320 + (i & 7))
        .endrepeat
keep_mask:
        .byte %00111111, %11001111, %11110011, %11111100
; Colors 0-3 at each of the four pixels of a byte
color_bits:
        .byte %00000000, %00000000, %00000000, %00000000
        .byte %01000000, %00010000, %00000100, %00000001
        .byte %10000000, %00100000, %00001000, %00000010
        .byte %11000000, %00110000, %00001100, %00000011

        .code
; Sets ptr2 to the byte with pixel A on row ptr1. Keeps Y, trashes A and X, and
; leaves the pixel position 0-3 in X.
pixel_address:
        tax
        and     #$FC
        asl     a               ; 8 bytes per 4 pixels, carry if over 255
        php
        clc
        adc     ptr1
        sta     ptr2
        lda     ptr1+1
        adc     #0
        plp
        adc     #0
        sta     ptr2+1
        txa
        and     #3
        tax
        rts

_gfx_rect:
        lda     _gfx_color      ; tmp4 = index of the color in color_bits
        asl     a
        asl     a
        sta     tmp4
        lda     _gfx_y
        sta     tmp1            ; tmp1 = row
        lda     _gfx_h
        sta     tmp2            ; tmp2 = rows left
@row:
        ldy     tmp1
        lda     row_lo,y
        sta     ptr1
        lda     row_hi,y
        sta     ptr1+1
        lda     _gfx_x
        sta     tmp3            ; tmp3 = x
        ldy     _gfx_w
@pixel:
        sty     @saved_y+1
        lda     tmp3
        jsr     pixel_address
        ldy     #0
        lda     (ptr2),y
        and     keep_mask,x
        sta     @merge+1
        txa
        ora     tmp4
        tax
        lda     color_bits,x
@merge: ora     #0
        sta     (ptr2),y
        inc     tmp3
@saved_y:
        ldy     #0
        dey
        bne     @pixel
        inc     tmp1
        dec     tmp2
        bne     @row
        rts

_gfx_get:
        ldy     _gfx_y
        lda     row_lo,y
        sta     ptr1
        lda     row_hi,y
        sta     ptr1+1
        lda     _gfx_x
        jsr     pixel_address
        ldy     #0
        lda     (ptr2),y
        and     mask_only,x
        beq     @done           ; zero is by far the most common answer
        ; Moves the two bits down
@shift: cpx     #3
        beq     @done
        lsr     a
        lsr     a
        inx
        bne     @shift
@done:
        ldx     #0              ; cc65 returns a 16-bit A/X
        rts

        .rodata
mask_only:
        .byte %11000000, %00110000, %00001100, %00000011

        .code
; Copies the shapes of screen code A to gfx_glyph_buf from the lower case set of
; the character ROM. Interrupts are off for the whole game, so banking is safe.
_gfx_glyph:
        ldx     #0
        stx     ptr1+1
        asl     a
        rol     ptr1+1
        asl     a
        rol     ptr1+1
        asl     a
        rol     ptr1+1
        sta     ptr1
        clc
        lda     ptr1+1
        adc     #>CHARGEN_LOWERCASE
        sta     ptr1+1
        lda     PORT
        pha
        and     #%11111011      ; CHAREN off, character ROM in at $D000
        sta     PORT
        ldy     #7
@copy:  lda     (ptr1),y
        sta     _gfx_glyph_buf,y
        dey
        bpl     @copy
        pla
        sta     PORT
        rts

        .export _rti_stub
; RESTORE ends up here
_rti_stub:
        rti
