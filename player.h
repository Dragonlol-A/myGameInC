#ifndef PLAYER_H
#define PLAYER_H

#define MAP_WIDTH 12
#define MAP_HEIGHT 12

typedef struct {
    double x;
    double y;
    double dirX;
    double dirY;
    double planeX;
    double planeY;
} Player;

extern const int worldMap[MAP_HEIGHT][MAP_WIDTH];

void initPlayer(Player *player);
void updatePlayer(
    Player *player,
    const int map[MAP_HEIGHT][MAP_WIDTH],
    double forward,
    double strafe,
    double turn,
    double deltaTime
);

#endif
