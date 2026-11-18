#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ScreenUpdate.h"
#include "player.h"

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("First WINDOW", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_SHOWN);

    if (win == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == NULL) {
        SDL_DestroyWindow(win);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Color *color = malloc(sizeof(Color));
    if( color == NULL) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    changeColor(color, 255, 192, 203); // Initialize color to pink

    //initialize Player
    Player *player = initPlayer(ren, win, "assets/placeHolder1.png", TEXTURE_WIDTH, TEXTURE_HEIGHT);
    if (player == NULL) {
        free(color);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        printf("Error while initializing player: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_QUIT) {
                running = false;
            }

            if( e.type == SDL_KEYDOWN ){ /* Look for a keypress */
                switch( e.key.keysym.sym ){
                    case SDLK_a:
                        printf("A key pressed\n");
                        changeColor(color , 0, 100, 0); // Change color to dark green
                        SDL_SetRenderDrawColor(ren, color->r, color->g, color->b, 255);
                        movePlayerTexture(ren, player);
                        break;
                    case SDLK_s:
                        printf("S key pressed\n");
                        player->y += 16;
                        movePlayerTexture(ren, player);
                        break;
                    default:
                        break;
                }
            }
        }
       
        SDL_SetRenderDrawColor(ren, color->r, color->g, color->b, 255); 
        SDL_RenderClear(ren);
        movePlayerTexture(ren, player); 
        // Present the backbuffer
        SDL_RenderPresent(ren);
    
        }
    // Cleanup
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
    
}