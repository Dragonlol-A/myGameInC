#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

#define MAP_WIDTH 12
#define MAP_HEIGHT 12

typedef struct {
    double x;
    double y;
    double dirX;
    double dirY;
    double planeX;
    double planeY;
    double planyZ;
} Player;

extern const int worldMap[MAP_HEIGHT][MAP_WIDTH];

bool canOccupyMap(const int map[MAP_HEIGHT][MAP_WIDTH], double x, double y, double radius);

void initPlayer(Player *player);

void updatePlayer(Player *player, const int map[MAP_HEIGHT][MAP_WIDTH], double forward, double strafe, double turn, double mouseDeltaX, double deltaTime);

void waponMovement(bool walking, double bobPhase, double bobAmount, SDL_Texture *woodenMacheteTexture, SDL_Renderer *renderer, int renderWidth, int renderHeight, double deltaTime);

#endif
