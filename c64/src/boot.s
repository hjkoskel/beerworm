; First file on the disk: shows the loading screen, loads the game behind it, and
; starts the game. The picture is in the same place as the game's own graphics (see
; c64.cfg), so the game's loading does not touch it.
;
; The picture is split in two files. The colors are loaded first, and then the
; bitmap builds up on the screen in the right colors, instead of half a minute of
; black screen on a 1541.
        .import __STAGE2_LOAD__, __STAGE2_RUN__, __STAGE2_SIZE__

; KERNAL
SETMSG  = $FF90
SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
DEVICE  = $BA           ; the drive this program was loaded from
CURSOR_OFF = $CC
POINTER = $FB           ; free zero page

PORT = $01
PORT_NO_BASIC = $36     ; lets the CPU read the RAM at $A000-$BFFF
PORT_DEFAULT = $37

VIC_CONTROL1 = $D011
VIC_RASTER = $D012
VIC_CONTROL2 = $D016
VIC_MEMORY = $D018
VIC_BORDER = $D020
VIC_BACKGROUND = $D021
CIA1_PORT_A = $DC00
CIA1_PORT_B = $DC01
CIA1_DDR_A = $DC02
CIA2_PORT_A = $DD00
CIA2_DDR_A = $DD02
COLOR_RAM = $D800
SCREEN_RAM = $8000

COLOR_RED = 2
GAME_START = $080D      ; after the BASIC line of the cc65 program
SPLASH_FRAMES = 250     ; at least 5 s of picture, for fast drives and emulators

BITMAP = $A000
; The rest of a Koala picture: screen colors, color RAM, background
PICTURE_SCREEN = BITMAP + 8000
PICTURE_COLORS = PICTURE_SCREEN + 1000
PICTURE_BACKGROUND = PICTURE_COLORS + 1000

        .segment "LOADADDR"
        .word   $0801

        .code
        ; 10 SYS2061
        .word   basic_end, 10
        .byte   $9E, "2061", 0
basic_end:
        .word   0

start:
        lda     #0
        sta     VIC_BORDER
        lda     VIC_CONTROL1    ; black until the picture is complete
        and     #%11101111
        sta     VIC_CONTROL1
        lda     #0
        jsr     SETMSG          ; no LOADING messages, they would change colors
        lda     #1
        sta     CURSOR_OFF
        lda     DEVICE
        cmp     #8
        bcs     @device_ok
        lda     #8
        sta     DEVICE
@device_ok:

        lda     #colors_name_end - colors_name
        ldx     #<colors_name
        ldy     #>colors_name
        jsr     SETNAM
        ldx     #<PICTURE_SCREEN
        ldy     #>PICTURE_SCREEN
        jsr     load_to_address

        lda     #PORT_NO_BASIC
        sta     PORT
        ldx     #0
@copy:  .repeat 4, part
        lda     PICTURE_SCREEN + part * 250,x
        sta     SCREEN_RAM + part * 250,x
        lda     PICTURE_COLORS + part * 250,x
        sta     COLOR_RAM + part * 250,x
        .endrepeat
        inx
        cpx     #250
        bne     @copy
        lda     PICTURE_BACKGROUND
        sta     VIC_BACKGROUND
        lda     #PORT_DEFAULT
        sta     PORT

        lda     #<BITMAP        ; empty bitmap, $A000-$BFFF is random after power on
        sta     POINTER
        lda     #>BITMAP
        sta     POINTER+1
        ldy     #0
        tya
@clear: sta     (POINTER),y
        iny
        bne     @clear
        inc     POINTER+1
        ldx     POINTER+1
        cpx     #>(BITMAP + $2000) ; also clears the loaded colors, they are copied
        bne     @clear

        lda     CIA2_DDR_A
        ora     #3
        sta     CIA2_DDR_A
        lda     CIA2_PORT_A
        and     #%11111100
        ora     #1              ; VIC bank 2, $8000-$BFFF
        sta     CIA2_PORT_A
        lda     #$08            ; screen at the start of the bank, bitmap at +$2000
        sta     VIC_MEMORY
        lda     #$18            ; multicolor
        sta     VIC_CONTROL2
        lda     #$3B            ; bitmap, screen on
        sta     VIC_CONTROL1

        lda     #bitmap_name_end - bitmap_name
        ldx     #<bitmap_name
        ldy     #>bitmap_name
        jsr     SETNAM
        ldx     #<BITMAP
        ldy     #>BITMAP
        jsr     load_to_address

        ldx     #<__STAGE2_SIZE__
@stage2:
        lda     __STAGE2_LOAD__ - 1,x
        sta     __STAGE2_RUN__ - 1,x
        dex
        bne     @stage2
        jmp     load_game

; The name is already set. X and Y are the address.
load_to_address:
        stx     POINTER
        sty     POINTER+1
        lda     #1
        ldx     DEVICE
        ldy     #0              ; to the address given to LOAD
        jsr     SETLFS
        lda     #0
        ldx     POINTER
        ldy     POINTER+1
        jsr     LOAD
        bcc     @ok
        lda     #COLOR_RED
        sta     VIC_BORDER
@stop:  jmp     @stop
@ok:    rts

colors_name:
        .byte   "KM.COLORS"
colors_name_end:
bitmap_name:
        .byte   "KM.BITMAP"
bitmap_name_end:

        .segment "STAGE2"
load_game:
        lda     #game_name_end - game_name
        ldx     #<game_name
        ldy     #>game_name
        jsr     SETNAM
        lda     #1
        ldx     DEVICE
        ldy     #1              ; load to the address in the file
        jsr     SETLFS
        lda     #0
        jsr     LOAD
        bcs     failed

        ; Slow drives have shown the picture for long, this is for fast ones
        sei                     ; stops the KERNAL from scanning the keys
        ldy     #SPLASH_FRAMES
@frame: lda     #0
        sta     CIA1_DDR_A      ; joystick in port 2: fire
        lda     CIA1_PORT_A
        and     #%00010000
        beq     @done
        lda     #$FF
        sta     CIA1_DDR_A
        lda     #0              ; any key
        sta     CIA1_PORT_A
        lda     CIA1_PORT_B
        cmp     #$FF
        bne     @done
@wait1: lda     VIC_RASTER
        cmp     #251
        bne     @wait1
@wait2: lda     VIC_RASTER
        cmp     #251
        beq     @wait2
        dey
        bne     @frame
@done:
        jmp     GAME_START

failed:
        lda     #COLOR_RED
        sta     VIC_BORDER
@stop:  jmp     @stop

game_name:
        .byte   "KM.GAME"
game_name_end:
