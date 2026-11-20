#include <stdbool.h>
#include <math.h>
#include "player.h"
#include "ScreenUpdate.h"

static bool isWall(const int map[MAP_HEIGHT][MAP_WIDTH], double x, double y) {
    int mapX = (int)floor(x);
    int mapY = (int)floor(y);

    if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
        return true;
    }
    return map[mapY][mapX] != 0;
}

bool canOccupyMap(const int map[MAP_HEIGHT][MAP_WIDTH], double x, double y, double radius) {
    return !isWall(map, x - radius, y - radius)
        && !isWall(map, x + radius, y - radius)
        && !isWall(map, x - radius, y + radius)
        && !isWall(map, x + radius, y + radius);
}

void initPlayer(Player *player) {
    player->x = 2.5;
    player->y = 2.5;
    player->dirX = 1.0;
    player->dirY = 0.0;
    player->planeX = 0.0;
    player->planeY = 0.66;
}

void updatePlayer(Player *player, const int map[MAP_HEIGHT][MAP_WIDTH], double forward, double strafe, double turn, double mouseDeltaX, double deltaTime) {
    const double moveSpeed = 3.0;
    const double rotationSpeed = 2.0;
    const double mouseSensitivity = 0.003;

    double rotation = turn * rotationSpeed * deltaTime - mouseDeltaX * mouseSensitivity;
    double cosRotation = cos(rotation);
    double sinRotation = sin(rotation);
    double oldDirX = player->dirX;
    double oldPlaneX = player->planeX;

    player->dirX = player->dirX * cosRotation - player->dirY * sinRotation;
    player->dirY = oldDirX * sinRotation + player->dirY * cosRotation;
    player->planeX = player->planeX * cosRotation - player->planeY * sinRotation;
    player->planeY = oldPlaneX * sinRotation + player->planeY * cosRotation;

    double inputLength = sqrt(forward * forward + strafe * strafe);
    if (inputLength > 1.0) {
        forward /= inputLength;
        strafe /= inputLength;
    }

    double moveX = (player->dirX * forward - player->dirY * strafe) * moveSpeed * deltaTime;
    double moveY = (player->dirY * forward + player->dirX * strafe) * moveSpeed * deltaTime;

    if (canOccupyMap(map, player->x + moveX, player->y, 0.2)) {
        player->x += moveX;
    }
    if (canOccupyMap(map, player->x, player->y + moveY, 0.2)) {
        player->y += moveY;
    }
}
