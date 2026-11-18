#include <stdio.h>
#include <SDL2/SDL.h>
#include "ScreenUpdate.h"
#include "player.h"

Player *initPlayer(SDL_Renderer *renderer, SDL_Window *window, const char *imagePath, int width, int height) {
    Player *player = malloc(sizeof(Player));
    if( player == NULL) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        fprintf(stderr, "Error while allocating player: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }
    player->x = 0;
    player->y = 0;
    player->width = width;
    player->height = height;
    player->texture = generateTexture(renderer, imagePath, width, height);
    if (player->texture == NULL) {
        free(player);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        fprintf(stderr, "Error while generating player texture: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }

    return player;
}
