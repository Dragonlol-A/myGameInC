#include <SDL2/SDL.h>
#include <stdio.h>
#include "player.h"

#define TEXTURE_WIDTH 64
#define TEXTURE_HEIGHT 64

typedef struct {
    Uint8 r;
    Uint8 g;
    Uint8 b;
} Color;

void changeColor(Color *c, int r, int g, int b); // change the color displayed at the screen

SDL_Texture *generateTexture(SDL_Renderer *renderer, const char *imagePath, int width, int height); // generate a texture with the specified color

void  movePlayerTexture(SDL_Renderer *renderer, Player *player); // move the player texture to a new position
