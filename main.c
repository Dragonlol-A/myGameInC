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

    #ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2"); //to avoid windows upscaling (mise à l'échelle)
    #endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Raycasting Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
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
    SDL_Texture *weaponSlashTexture = loadTextures(renderer, "assets/waponSlash.png");
    SDL_Texture *settingsButtonTexture = loadTextures(renderer, "assets/SettingsButton.png");
    if (defaultWallTexture == NULL || theBestWallTexture == NULL || enemyTexture == NULL
        || woodenMacheteTexture == NULL || weaponSlashTexture == NULL
        || settingsButtonTexture == NULL) {
        fprintf(stderr, "Failed to load one or more textures.\n");
        if (defaultWallTexture != NULL) SDL_DestroyTexture(defaultWallTexture);
        if (theBestWallTexture != NULL) SDL_DestroyTexture(theBestWallTexture);
        if (enemyTexture != NULL) SDL_DestroyTexture(enemyTexture);
        if (woodenMacheteTexture != NULL) SDL_DestroyTexture(woodenMacheteTexture);
        if (weaponSlashTexture != NULL) SDL_DestroyTexture(weaponSlashTexture);
        if (settingsButtonTexture != NULL) SDL_DestroyTexture(settingsButtonTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    SDL_SetTextureBlendMode(woodenMacheteTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(weaponSlashTexture, SDL_BLENDMODE_BLEND);

    double bobPhase = 0.0;
    double bobAmount = 0.0;
    const double attackDuration = 0.42;
    const double attackCooldownDuration = 0.65;
    double attackTimer = 0.0;
    double attackCooldownTimer = 0.0;

    if (SDL_QueryTexture(defaultWallTexture, NULL, NULL, &textureWidth, &textureHeight) != 0) {
        fprintf(stderr, "SDL_QueryTexture Error: %s\n", SDL_GetError());
        if (defaultWallTexture != NULL) SDL_DestroyTexture(defaultWallTexture);
        if (theBestWallTexture != NULL) SDL_DestroyTexture(theBestWallTexture);
        if (enemyTexture != NULL) SDL_DestroyTexture(enemyTexture);
        if (woodenMacheteTexture != NULL) SDL_DestroyTexture(woodenMacheteTexture);
        if (weaponSlashTexture != NULL) SDL_DestroyTexture(weaponSlashTexture);
        if (settingsButtonTexture != NULL) SDL_DestroyTexture(settingsButtonTexture);
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
        SDL_DestroyTexture(defaultWallTexture);
        SDL_DestroyTexture(theBestWallTexture);
        SDL_DestroyTexture(enemyTexture);
        SDL_DestroyTexture(woodenMacheteTexture);
        SDL_DestroyTexture(weaponSlashTexture);
        SDL_DestroyTexture(settingsButtonTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Enemy enemy;
    initEnemy(&enemy, DEFALT_ENEMY_POS_X, DEFALT_ENEMY_POS_Y, enemyTexture, enemyWidth, enemyHeight);

    bool running = true;
    SDL_Texture *sceneTexture = NULL;
    int sceneTextureWidth = 0;
    int sceneTextureHeight = 0;
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
                mouseDeltaX -= event.motion.xrel;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (attackCooldownTimer <= 0.0 && attackTimer <= 0.0) {
                    attackTimer = attackDuration;
                    attackCooldownTimer = attackCooldownDuration;
                }
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat
                && event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                if (attackCooldownTimer <= 0.0 && attackTimer <= 0.0) {
                    attackTimer = attackDuration;
                    attackCooldownTimer = attackCooldownDuration;
                }
            }
        }

        Uint64 currentCounter = SDL_GetPerformanceCounter();
        double deltaTime = (double)(currentCounter - previousCounter) / counterFrequency;
        previousCounter = currentCounter;

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        double forward = (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_UP] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] ? 1.0 : 0.0);
        double strafe = (keys[SDL_SCANCODE_D] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_Q] ? 1.0 : 0.0);
        double turn = (keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0) - (keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);

        if(keys[SDL_SCANCODE_ESCAPE]){
            if (openMenu(renderer, window, settingsButtonTexture) == 1) running = false;
            previousCounter = SDL_GetPerformanceCounter();
        }
        if (!running) break;

        attackTimer = fmax(0.0, attackTimer - deltaTime);
        attackCooldownTimer = fmax(0.0, attackCooldownTimer - deltaTime);
        bool attacking = attackTimer > 0.0;
        double attackProgress = attacking ? 1.0 - attackTimer / attackDuration : 0.0;

        int renderWidth;
        int renderHeight;
        getRenderResolution(&renderWidth, &renderHeight);
        if (sceneTexture == NULL || renderWidth != sceneTextureWidth
            || renderHeight != sceneTextureHeight) {
            SDL_Texture *newSceneTexture = SDL_CreateTexture(
                renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                renderWidth, renderHeight);
            if (newSceneTexture == NULL) {
                fprintf(stderr, "SDL_CreateTexture render target Error: %s\n", SDL_GetError());
                running = false;
                break;
            }
            if (SDL_SetTextureBlendMode(newSceneTexture, SDL_BLENDMODE_NONE) != 0) {
                fprintf(stderr, "SDL_SetTextureBlendMode render target Error: %s\n", SDL_GetError());
                SDL_DestroyTexture(newSceneTexture);
                running = false;
                break;
            }

            if (sceneTexture != NULL) {
                SDL_DestroyTexture(sceneTexture);
            }
            sceneTexture = newSceneTexture;
            sceneTextureWidth = renderWidth;
            sceneTextureHeight = renderHeight;
        }

        double oldX = player.x;
        double oldY = player.y;

        updatePlayer(&player, worldMap, forward, strafe, turn , mouseDeltaX , deltaTime);
        bool walking = hypot(player.x - oldX, player.y - oldY) > 0.0001;
        updateEnemy(&enemy, &player, worldMap, deltaTime);
        if (SDL_SetRenderTarget(renderer, sceneTexture) != 0) {
            fprintf(stderr, "SDL_SetRenderTarget Error: %s\n", SDL_GetError());
            running = false;
            break;
        }
        bool sceneRendered = renderScene(renderer, &player, defaultWallTexture,
            theBestWallTexture, &enemy, textureWidth, textureHeight,
            renderWidth, renderHeight, worldMap);
        
        if (walking) bobPhase += deltaTime * 10.0;
        if (sceneRendered) {
            waponMovement(walking, bobPhase, bobAmount, woodenMacheteTexture,
                weaponSlashTexture, renderer, renderWidth, renderHeight,
                deltaTime, attacking, attackProgress);
        }

        if (SDL_SetRenderTarget(renderer, NULL) != 0) {
            fprintf(stderr, "SDL_SetRenderTarget reset Error: %s\n", SDL_GetError());
            running = false;
            break;
        }
        if (!sceneRendered) {
            running = false;
            break;
        }
        if (SDL_RenderCopy(renderer, sceneTexture, NULL, NULL) != 0) {
            fprintf(stderr, "SDL_RenderCopy render target Error: %s\n", SDL_GetError());
            running = false;
            break;
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(defaultWallTexture);
    SDL_DestroyTexture(theBestWallTexture);
    SDL_DestroyTexture(enemy.texture);
    SDL_DestroyTexture(woodenMacheteTexture);
    SDL_DestroyTexture(weaponSlashTexture);
    SDL_DestroyTexture(settingsButtonTexture);
    SDL_DestroyTexture(sceneTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
