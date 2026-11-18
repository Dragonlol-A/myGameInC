CC := gcc
TARGET := game
SRC := main.c ScreenUpdate.c player.c

CFLAGS := $(shell pkg-config --cflags sdl2) -Wall -Wextra
LDLIBS := $(shell pkg-config --libs sdl2 SDL2_image)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET)