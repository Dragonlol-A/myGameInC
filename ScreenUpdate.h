#ifndef SCREEN_UPDATE_H
#define SCREEN_UPDATE_H

#include <SDL2/SDL.h>
#include "player.h"
#include "enemy.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

void renderScene(SDL_Renderer *renderer, const Player *player, SDL_Texture *wallTexture, SDL_Texture *theBestWallTexture, const Enemy *enemy, int textureWidth, int textureHeight, const int map[MAP_HEIGHT][MAP_WIDTH]);

SDL_Texture *loadTextures(SDL_Renderer *renderer, const char *texturePath);

int openMenu(SDL_Renderer *renderer);

#endif
