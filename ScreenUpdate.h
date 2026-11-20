#ifndef SCREEN_UPDATE_H
#define SCREEN_UPDATE_H

#include <stdbool.h>
#include "gl_renderer.h"
#include "player.h"
#include "enemy.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

void getRenderResolution(int *width, int *height);

bool renderScene(const Player *player, GLuint wallTexture, GLuint theBestWallTexture,
    const Enemy *enemy, int textureWidth, int textureHeight,
    int renderWidth, int renderHeight, const int map[MAP_HEIGHT][MAP_WIDTH]);

int openMenu(SDL_Window *window, GLuint settingsButtonTexture);
void drawWeapon(bool walking, double bobPhase, double bobAmount, GLuint weaponTexture, int weaponTextureWidth, int weaponTextureHeight, GLuint slashTexture, int renderWidth, int renderHeight, double deltaTime, bool attacking, double attackProgress);

#endif
