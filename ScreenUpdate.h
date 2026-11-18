#ifndef SCREEN_UPDATE_H
#define SCREEN_UPDATE_H

#include <SDL2/SDL.h>
#include "player.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

void renderScene(SDL_Renderer *renderer, const Player *player, SDL_Texture *wallTexture, int textureWidth, int textureHeight, const int map[MAP_HEIGHT][MAP_WIDTH]);

SDL_Texture *loadTextures(SDL_Renderer *renderer);

int openMenu(SDL_Renderer *renderer);

#endif
