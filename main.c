#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <math.h>
#include "ScreenUpdate.h"
#include "player.h"
#include "enemy.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

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
    SDL_Texture *defaultWallTexture = loadTextures(renderer, "assets/wall.png");
    SDL_Texture *theBestWallTexture = loadTextures(renderer, "assets/theBestWall.png");
    SDL_Texture *enemyTexture = loadTextures(renderer, "assets/grinch.png");
    SDL_Texture *woodenMacheteTexture = loadTextures(renderer, "assets/wood_machete.png");
    if (defaultWallTexture == NULL || theBestWallTexture == NULL || enemyTexture == NULL || woodenMacheteTexture == NULL) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    

    
    SDL_SetTextureBlendMode(woodenMacheteTexture, SDL_BLENDMODE_BLEND);

    double bobPhase = 0.0;
    double bobAmount = 0.0;

    if (SDL_QueryTexture(defaultWallTexture, NULL, NULL, &textureWidth, &textureHeight) != 0) {
        fprintf(stderr, "SDL_QueryTexture Error: %s\n", SDL_GetError());
        SDL_DestroyTexture(defaultWallTexture);
        SDL_DestroyTexture(theBestWallTexture);
        SDL_DestroyTexture(enemyTexture);
        SDL_DestroyTexture(woodenMacheteTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Player player;
    initPlayer(&player);

    int enemyWidth, enemyHeight;
    if (SDL_QueryTexture(enemyTexture, NULL, NULL, &enemyWidth, &enemyHeight) != 0) {
        fprintf(stderr, "SDL_QueryTexture enemy Error: %s\n", SDL_GetError());
        return 1;
    }

    Enemy enemy;
    initEnemy(&enemy, DEFALT_ENEMY_POS_X, DEFALT_ENEMY_POS_Y, enemyTexture, enemyWidth, enemyHeight);

    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const double counterFrequency = (double)SDL_GetPerformanceFrequency();
    //set the mouse to relative mode to capture mouse movement (hide the cursor and get relative motion)
    SDL_SetRelativeMouseMode(SDL_TRUE);
    while (running) {
        SDL_Event event;
        int mouseDeltaX = 0;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                mouseDeltaX += event.motion.xrel;
            }
        }

        Uint64 currentCounter = SDL_GetPerformanceCounter();
        double deltaTime = (double)(currentCounter - previousCounter) / counterFrequency;
        previousCounter = currentCounter;

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        double forward = (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_UP] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] ? 1.0 : 0.0);
        double strafe = (keys[SDL_SCANCODE_D] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_Q] ? 1.0 : 0.0);
        double turn = (keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);
        double oldX = player.x;
        double oldY = player.y;

        updatePlayer(&player, worldMap, forward, strafe, turn , mouseDeltaX , deltaTime);
        bool walking = hypot(player.x - oldX, player.y - oldY) > 0.0001;
        printf("Player Position: (%.2f, %.2f), Walking: %s\n", player.x, player.y, walking ? "true" : "false"); // Debugging line to check player position and walking state
        updateEnemy(&enemy, &player, worldMap, deltaTime);
        renderScene(renderer, &player,defaultWallTexture, theBestWallTexture, &enemy, textureWidth, textureHeight, worldMap);
        
        if (walking) bobPhase += deltaTime * 10.0;
        waponMovement(walking, bobPhase, bobAmount, woodenMacheteTexture, renderer, deltaTime);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(defaultWallTexture);
    SDL_DestroyTexture(theBestWallTexture);
    SDL_DestroyTexture(enemy.texture);
    SDL_DestroyTexture(woodenMacheteTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
