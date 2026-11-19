#ifndef SCREEN_UPDATE_H
#define SCREEN_UPDATE_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "player.h"
#include "enemy.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

void getRenderResolution(int *width, int *height);

bool renderScene(SDL_Renderer *renderer, const Player *player, SDL_Texture *wallTexture, SDL_Texture *theBestWallTexture, const Enemy *enemy, int textureWidth, int textureHeight, int renderWidth, int renderHeight, const int map[MAP_HEIGHT][MAP_WIDTH]);

SDL_Texture *loadTextures(SDL_Renderer *renderer, const char *texturePath);

int openMenu(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture *settingsButtonTexture);

#endif
