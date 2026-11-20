#ifndef ENEMY_H
#define ENEMY_H

#include <SDL2/SDL_opengl.h>
#include "player.h"

#define DEFALT_ENEMY_POS_X 5.5
#define DEFALT_ENEMY_POS_Y 5.5

typedef struct {
    double x;
    double y;
    GLuint texture;
    int textureWidth;
    int textureHeight;
} Enemy;

void initEnemy(Enemy *enemy, double x, double y, GLuint texture, int textureWidth, int textureHeight);

void updateEnemy(Enemy *enemy, const Player *player, const int map[MAP_HEIGHT][MAP_WIDTH], double deltaTime);

#endif
