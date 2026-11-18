#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL_image.h>
#include "ScreenUpdate.h"

const int worldMap[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,1,1,0,0,1,0,0,1},
    {1,0,0,0,1,0,0,0,1,0,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,1,1,0,0,0,1},
    {1,0,1,0,0,0,0,0,0,1,0,1},
    {1,0,1,0,0,0,0,0,0,1,0,1},
    {1,0,0,0,0,1,0,0,0,0,0,1},
    {1,0,0,0,0,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1}
};

static void setDrawColor(SDL_Renderer *renderer, Uint8 red, Uint8 green, Uint8 blue) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
}

SDL_Texture *loadTextures(SDL_Renderer *renderer) {
    SDL_Surface *surface = IMG_Load("assets/wall.png");

    if (surface == NULL) {
        fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
        return NULL;
    }

    SDL_Texture *wallTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (wallTexture == NULL) {
        fprintf(stderr, "Texture creation error: %s\n", SDL_GetError());
        return NULL;
    }
    return wallTexture;
}

void renderScene(SDL_Renderer *renderer, const Player *player, SDL_Texture *wallTexture, int textureWidth, int textureHeight, const int map[MAP_HEIGHT][MAP_WIDTH]) {

    setDrawColor(renderer, 42, 48, 68);
    SDL_Rect ceiling = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_RenderFillRect(renderer, &ceiling);

    setDrawColor(renderer, 62, 54, 45);
    SDL_Rect floorRect = {0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2};
    SDL_RenderFillRect(renderer, &floorRect);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double cameraX = 2.0 * x / SCREEN_WIDTH - 1.0;
        double rayDirX = player->dirX + player->planeX * cameraX;
        double rayDirY = player->dirY + player->planeY * cameraX;
        int mapX = (int)player->x;
        int mapY = (int)player->y;

        double deltaDistX = rayDirX == 0.0 ? 1e30 : fabs(1.0 / rayDirX);
        double deltaDistY = rayDirY == 0.0 ? 1e30 : fabs(1.0 / rayDirY);
        double sideDistX;
        double sideDistY;
        int stepX;
        int stepY;

        if (rayDirX < 0.0) {
            stepX = -1;
            sideDistX = (player->x - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - player->x) * deltaDistX;
        }
        if (rayDirY < 0.0) {
            stepY = -1;
            sideDistY = (player->y - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - player->y) * deltaDistY;
        }
        
        int side = 0;
        bool hit = false;
        for (int steps = 0; steps < MAP_WIDTH * MAP_HEIGHT; steps++) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT || map[mapY][mapX] != 0) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            continue;
        }

        double perpendicularDistance;
        if (side == 0) {
            perpendicularDistance = (mapX - player->x + (1 - stepX) / 2.0) / rayDirX;
        } else {
            perpendicularDistance = (mapY - player->y + (1 - stepY) / 2.0) / rayDirY;
        }
        if (perpendicularDistance < 0.1) {
            perpendicularDistance = 0.1;
        }

        double wallX;
        if (side == 0) {
            wallX = player->y + perpendicularDistance * rayDirY;
        } else {
            wallX = player->x + perpendicularDistance * rayDirX;
        }
        wallX -= floor(wallX);

        int textureX = (int)(wallX * textureWidth);

        if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0)) {
            textureX = textureWidth - textureX - 1;
        }

        int lineHeight = (int)(SCREEN_HEIGHT / perpendicularDistance);
        if (lineHeight < 1) {
            lineHeight = 1;
        }

        int projectedStart = SCREEN_HEIGHT / 2 - lineHeight / 2;
        int projectedEnd = projectedStart + lineHeight;
        int drawStart = projectedStart < 0 ? 0 : projectedStart;
        int drawEnd = projectedEnd > SCREEN_HEIGHT ? SCREEN_HEIGHT : projectedEnd;
        if (drawEnd <= drawStart) {
            continue;
        }

        int sourceStart = (int)((drawStart - projectedStart) * (double)textureHeight / lineHeight);
        int sourceEnd = (int)((drawEnd - projectedStart) * (double)textureHeight / lineHeight);
        if (sourceStart < 0) {
            sourceStart = 0;
        }
        if (sourceEnd > textureHeight) {
            sourceEnd = textureHeight;
        }
        if (sourceEnd <= sourceStart) {
            sourceEnd = sourceStart + 1;
        }

        double brightness = 1.0 / (1.0 + perpendicularDistance * 0.12);
        Uint8 shade = (Uint8)(255.0 * brightness);
        
        if (side == 1) {
            brightness *= 0.72;
        }

        SDL_SetTextureColorMod(wallTexture, shade, shade, shade);
        SDL_Rect source = {textureX, sourceStart, 1, sourceEnd - sourceStart};
        SDL_Rect destination = {x, drawStart, 1, drawEnd - drawStart};
        SDL_RenderCopy(renderer, wallTexture, &source, &destination);
    }

    SDL_SetTextureColorMod(wallTexture, 255, 255, 255);
}

int openMenu(SDL_Renderer *renderer) {
    bool onMenu = 1;
    
    while (onMenu) {
        setDrawColor(renderer, 0, 0, 0);
        SDL_Rect optionButton = {SCREEN_WIDTH/2 - SCREEN_WIDTH/16, SCREEN_HEIGHT/2 - SCREEN_HEIGHT/16, SCREEN_WIDTH/8, SCREEN_HEIGHT/8};
        SDL_RenderFillRect(renderer, NULL);
        setDrawColor(renderer, 143, 200, 120);
        SDL_RenderFillRect(renderer, &optionButton);
        SDL_RenderPresent(renderer);


        SDL_Event event;
       while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 1;
            }

            if (event.type == SDL_KEYDOWN &&
                !event.key.repeat &&
                event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                onMenu = 0;
            }
        }
        SDL_Delay(100);
        
    }

    return 0;
}

