#ifndef PLAYER_H
#define PLAYER_H
#include <SDL2/SDL.h>
#include <stdio.h>

typedef struct {
    int x;
    int y;
    int width;
    int height;
    SDL_Texture *texture;
} Player;

Player *initPlayer(SDL_Renderer *renderer, SDL_Window *window, const char *imagePath, int width, int height);

#endif 