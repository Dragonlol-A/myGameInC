CC := gcc
TARGET := game
SRC := main.c ScreenUpdate.c player.c enemy.c gl_renderer.c model.c gltf_model.c

CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer) -Wall -Wextra
LDLIBS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer gl) -lm

WINDOWS_CC ?= x86_64-w64-mingw32-gcc
WINDOWS_TARGET := game.exe
WINDOWS_SDL2_ROOT ?= SDL2_for_windows/SDL2-2.32.2/x86_64-w64-mingw32
WINDOWS_SDL2_IMAGE_ROOT ?= SDL2_for_windows/SDL2_image-2.8.12/x86_64-w64-mingw32
WINDOWS_SDL2_MIXER_ROOT ?= SDL2_for_windows/SDL2_mixer-2.8.1/x86_64-w64-mingw32
WINDOWS_SDL2_DLL := $(WINDOWS_SDL2_ROOT)/bin/SDL2.dll
WINDOWS_SDL2_IMAGE_DLL := $(WINDOWS_SDL2_IMAGE_ROOT)/bin/SDL2_image.dll
WINDOWS_SDL2_MIXER_DLL := $(WINDOWS_SDL2_MIXER_ROOT)/bin/SDL2_mixer.dll
WINDOWS_CFLAGS ?= -I$(WINDOWS_SDL2_ROOT)/include -I$(WINDOWS_SDL2_ROOT)/include/SDL2 \
	-I$(WINDOWS_SDL2_IMAGE_ROOT)/include -I$(WINDOWS_SDL2_IMAGE_ROOT)/include/SDL2 \
	-I$(WINDOWS_SDL2_MIXER_ROOT)/include -I$(WINDOWS_SDL2_MIXER_ROOT)/include/SDL2 \
	-Dmain=SDL_main
WINDOWS_LDLIBS ?= -L$(WINDOWS_SDL2_MIXER_ROOT)/lib -L$(WINDOWS_SDL2_IMAGE_ROOT)/lib -L$(WINDOWS_SDL2_ROOT)/lib \
	-lmingw32 -lSDL2main -lSDL2_mixer -lSDL2_image -lSDL2 -lopengl32 -mwindows -lm

.PHONY: all clean windows windows-deps

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

windows: $(WINDOWS_TARGET)

windows-deps:
	@command -v $(WINDOWS_CC) >/dev/null || { echo "Missing Windows compiler: $(WINDOWS_CC)"; exit 1; }
	@test -f "$(WINDOWS_SDL2_ROOT)/include/SDL2/SDL.h" || { echo "SDL2 headers not found under $(WINDOWS_SDL2_ROOT)"; exit 1; }
	@test -f "$(WINDOWS_SDL2_IMAGE_ROOT)/include/SDL2/SDL_image.h" || { echo "Install the x64 MinGW SDL2_image development package and set WINDOWS_SDL2_IMAGE_ROOT"; exit 1; }
	@test -f "$(WINDOWS_SDL2_MIXER_ROOT)/include/SDL2/SDL_mixer.h" || { echo "Install the x64 MinGW SDL2_mixer development package and set WINDOWS_SDL2_MIXER_ROOT"; exit 1; }
	@test -f "$(WINDOWS_SDL2_ROOT)/lib/libSDL2.dll.a" || { echo "SDL2 MinGW import library not found under $(WINDOWS_SDL2_ROOT)/lib"; exit 1; }
	@test -f "$(WINDOWS_SDL2_IMAGE_ROOT)/lib/libSDL2_image.dll.a" -o -f "$(WINDOWS_SDL2_IMAGE_ROOT)/lib/libSDL2_image.a" || { echo "SDL2_image MinGW library not found under $(WINDOWS_SDL2_IMAGE_ROOT)/lib"; exit 1; }
	@test -f "$(WINDOWS_SDL2_MIXER_ROOT)/lib/libSDL2_mixer.dll.a" -o -f "$(WINDOWS_SDL2_MIXER_ROOT)/lib/libSDL2_mixer.a" || { echo "SDL2_mixer MinGW library not found under $(WINDOWS_SDL2_MIXER_ROOT)/lib"; exit 1; }
	@test -f "$(WINDOWS_SDL2_DLL)" || { echo "SDL2 runtime DLL not found: $(WINDOWS_SDL2_DLL)"; exit 1; }
	@test -f "$(WINDOWS_SDL2_IMAGE_DLL)" || { echo "SDL2_image runtime DLL not found: $(WINDOWS_SDL2_IMAGE_DLL)"; exit 1; }
	@test -f "$(WINDOWS_SDL2_MIXER_DLL)" || { echo "SDL2_mixer runtime DLL not found: $(WINDOWS_SDL2_MIXER_DLL)"; exit 1; }

$(WINDOWS_TARGET): $(SRC) windows-deps
	$(WINDOWS_CC) -Wall -Wextra $(WINDOWS_CFLAGS) $(SRC) -o $@ $(WINDOWS_LDLIBS)
	cp "$(WINDOWS_SDL2_DLL)" .
	cp "$(WINDOWS_SDL2_IMAGE_DLL)" .
	cp "$(WINDOWS_SDL2_MIXER_ROOT)"/bin/*.dll .

clean:
	rm -f $(TARGET) $(WINDOWS_TARGET)