#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "enemy.h"

void initEnemy(Enemy *enemy, double x, double y, GLuint texture, int textureWidth, int textureHeight) {
    enemy->x = x;
    enemy->y = y;
    enemy->texture = texture;
    enemy->textureWidth = textureWidth;
    enemy->textureHeight = textureHeight;
}

void updateEnemy(Enemy *enemy, const Player *player, const int map[MAP_HEIGHT][MAP_WIDTH], double deltaTime) {
    const double moveSpeed = 1.2;
    const double stopDistance = 0.6;
    const double collisionRadius = 0.2;
    const double maxStep = 0.1;

    double directionX = player->x - enemy->x;
    double directionY = player->y - enemy->y;
    double distance = hypot(directionX, directionY);
    if (distance <= stopDistance || deltaTime <= 0.0) {
        return;
    }

    double travelDistance = fmin(moveSpeed * deltaTime, distance - stopDistance);
    double moveX = directionX / distance * travelDistance;
    double moveY = directionY / distance * travelDistance;
    int steps = (int)ceil(fmax(fabs(moveX), fabs(moveY)) / maxStep);
    double stepX = moveX / steps;
    double stepY = moveY / steps;

    for (int step = 0; step < steps; step++) {
        if (canOccupyMap(map, enemy->x + stepX, enemy->y, collisionRadius)) {
            enemy->x += stepX;
        }
        if (canOccupyMap(map, enemy->x, enemy->y + stepY, collisionRadius)) {
            enemy->y += stepY;
        }
    }
}