CL65 := cl65
BUILD := build
PRG := $(BUILD)/kaljamato.prg
BOOT := $(BUILD)/boot.prg
D64 := $(BUILD)/kaljamato.d64
SPLASH := src/splashload.kla

GFX_DIR := ../src/gfx
SND_DIR := ../src/snd
ASSET_INPUTS := tools/gen_assets.py $(wildcard $(GFX_DIR)/*.png) $(wildcard $(SND_DIR)/*.mp3)

# Not plain -O: see the note on cc65 2.18 in src/game.c
CFLAGS := -Oi -I src -I $(BUILD)

GAME_OBJS := $(BUILD)/main.o $(BUILD)/game.o $(BUILD)/gfx.o $(BUILD)/sound.o $(BUILD)/assets.o
TEST_OBJS := $(BUILD)/test/test_game.o $(BUILD)/test/fake_gfx.o $(BUILD)/test/game.o $(BUILD)/test/assets.o

all: $(PRG) $(D64)

$(PRG): $(GAME_OBJS) c64.cfg
	$(CL65) -t c64 -C c64.cfg -m $(BUILD)/kaljamato.map -o $@ $(GAME_OBJS)

$(BOOT): src/boot.s boot.cfg | $(BUILD)
	$(CL65) -t none -C boot.cfg -o $@ src/boot.s

# A Koala file is a 2 byte address, 8000 bytes of bitmap, then 2001 bytes of colors.
# boot.s loads the parts to its own addresses, but each file still needs the 2 bytes.
$(BUILD)/splash_bitmap.bin: $(SPLASH) | $(BUILD)
	head -c 8002 $< > $@

$(BUILD)/splash_colors.bin: $(SPLASH) | $(BUILD)
	(head -c 2 $<; tail -c 2001 $<) > $@

# The boot program is first, for LOAD"*",8,1. The names are also in src/boot.s.
$(D64): $(BOOT) $(PRG) $(BUILD)/splash_bitmap.bin $(BUILD)/splash_colors.bin
	rm -f $@
	c1541 -format kaljamato,km d64 $@ -write $(BOOT) kaljamato \
		-write $(BUILD)/splash_colors.bin km.colors \
		-write $(BUILD)/splash_bitmap.bin km.bitmap \
		-write $(PRG) km.game >/dev/null

$(BUILD)/assets.s $(BUILD)/assets.h &: $(ASSET_INPUTS) | $(BUILD)
	python3 tools/gen_assets.py $(GFX_DIR) $(SND_DIR) $(BUILD)

$(BUILD)/%.o: src/%.c src/game.h src/gfx.h src/sound.h $(BUILD)/assets.h | $(BUILD)
	$(CL65) -t c64 $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: src/%.s | $(BUILD)
	$(CL65) -t c64 -c -o $@ $<

$(BUILD)/assets.o: $(BUILD)/assets.s
	$(CL65) -t c64 -c -o $@ $<

# The tests run the 6502 code in the sim65 simulator, with a fake bitmap
$(BUILD)/test/%.o: tests/%.c src/game.h src/gfx.h $(BUILD)/assets.h | $(BUILD)/test
	$(CL65) -t sim6502 $(CFLAGS) -c -o $@ $<

$(BUILD)/test/game.o: src/game.c src/game.h src/gfx.h $(BUILD)/assets.h | $(BUILD)/test
	$(CL65) -t sim6502 $(CFLAGS) -c -o $@ $<

$(BUILD)/test/assets.o: $(BUILD)/assets.s | $(BUILD)/test
	$(CL65) -t sim6502 -c -o $@ $<

$(BUILD)/test_game: $(TEST_OBJS)
	$(CL65) -t sim6502 -o $@ $(TEST_OBJS)

$(BUILD) $(BUILD)/test:
	mkdir -p $@

run: $(D64)
	x64sc -autostart $(D64)

# Only the game, without loading from the disk
run-prg: $(PRG)
	x64sc -autostart $(PRG)

test: $(BUILD)/test_game
	sim65 $(BUILD)/test_game

clean:
	rm -rf $(BUILD)

.PHONY: all run run-prg test clean
