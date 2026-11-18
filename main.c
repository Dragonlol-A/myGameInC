#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ScreenUpdate.h"
#include "player.h"

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Raycasting Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int textureWidth;
    int textureHeight;
    SDL_Texture *wallTexture = loadTextures(renderer);
    if (wallTexture == NULL) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (SDL_QueryTexture(wallTexture, NULL, NULL, &textureWidth, &textureHeight) != 0) {
        fprintf(stderr, "SDL_QueryTexture Error: %s\n", SDL_GetError());
        SDL_DestroyTexture(wallTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Player player;
    initPlayer(&player);

    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const double counterFrequency = (double)SDL_GetPerformanceFrequency();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        Uint64 currentCounter = SDL_GetPerformanceCounter();
        double deltaTime = (double)(currentCounter - previousCounter) / counterFrequency;
        previousCounter = currentCounter;

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        double forward = (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_UP] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] ? 1.0 : 0.0);
        double strafe = (keys[SDL_SCANCODE_D] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_Q] ? 1.0 : 0.0);
        double turn = (keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);
        
        if (keys[SDL_SCANCODE_ESCAPE]) if (openMenu(renderer) == 1) {
            running = false;
        }

        updatePlayer(&player, worldMap, forward, strafe, turn, deltaTime);
        renderScene(renderer, &player,wallTexture, textureWidth, textureHeight, worldMap);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(wallTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
