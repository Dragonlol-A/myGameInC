#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "player.h"

#define TEXTURE_WIDTH 64
#define TEXTURE_HEIGHT 64

typedef struct {
    Uint8 r;
    Uint8 g;
    Uint8 b;
} Color;

void changeColor(Color *c, int r, int g, int b) {
    c->r = (Uint8)r;
    c->g = (Uint8)g;
    c->b = (Uint8)b;
}

SDL_Texture *generateTexture(SDL_Renderer *renderer, const char *imagePath, int width, int height) {
    SDL_Surface *surface = IMG_Load(imagePath);

    if (!surface)
    {
        printf("IMG_Load Error: %s\n", IMG_GetError());
        return NULL;
    }

    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    if (!texture)
    {
        printf("SDL_CreateTextureFromSurface Error: %s\n",
               SDL_GetError());
        return NULL;
    }

    SDL_Rect dst = {0, 0, width, height};

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_RenderPresent(renderer);

    //SDL_DestroyTexture(texture);

    return texture;
}

void movePlayerTexture(SDL_Renderer *renderer, Player *player) {
    SDL_Rect dst = {player->x, player->y, player->width, player->height};



    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, player->texture, NULL, &dst);
    SDL_RenderPresent(renderer);
}
