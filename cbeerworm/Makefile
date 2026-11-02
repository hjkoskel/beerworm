CC ?= gcc
BUILD := build
TARGET := $(BUILD)/kaljamato

# -mgeneral-regs-only makes the compiler reject any floating point in the game
# code: no float can sneak in. Use "make NOFLOAT=" on a compiler without it.
NOFLOAT ?= -mgeneral-regs-only

CFLAGS ?= -O2
CFLAGS += -std=gnu23 -Wall -Wextra -Wconversion -Wsign-conversion $(NOFLOAT)
CFLAGS += $(shell pkg-config --cflags raylib)
LDLIBS += $(shell pkg-config --libs raylib)

LOGIC := src/game.c src/imath.c src/settings.c
HEADERS := $(wildcard src/*.h)
ASSETS := $(wildcard gfx/*.png) $(wildcard snd/*.mp3)

all: $(TARGET)

$(TARGET): src/main.c $(LOGIC) $(HEADERS) $(ASSETS) | $(BUILD)
	$(CC) $(CFLAGS) src/main.c $(LOGIC) -o $@ $(LDLIBS)

$(BUILD)/test_game: tests/test_game.c $(LOGIC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) tests/test_game.c $(LOGIC) -o $@

$(BUILD):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET) $(ARGS)

test: $(BUILD)/test_game
	./$(BUILD)/test_game

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
