#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <math.h>
#include "ScreenUpdate.h"
#include "player.h"
#include "enemy.h"
#include "model.h"

#ifdef _WIN32
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

int main(int argc, char *argv[]) {
    const char *modelPath = "assets/3dAssets/gnome/garden_gnome_4k.gltf";

    #ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2"); //to avoid windows upscaling (mise à l'échelle)
    #endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) != 0 || SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1) != 0 || SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
        fprintf(stderr, "SDL_GL_SetAttribute Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Raycasting Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (glContext == NULL) {
        fprintf(stderr, "SDL_GL_CreateContext Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!glRendererInitialize()) {
        glRendererShutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (glSetSwapInterval(true) != 0) {
        fprintf(stderr, "SDL_GL_SetSwapInterval Error: %s\n", SDL_GetError());
    }

    int textureWidth;
    int textureHeight;
    int enemyWidth;
    int enemyHeight;
    int weaponTextureWidth;
    int weaponTextureHeight;
    GLuint defaultWallTexture = glLoadTexture("assets/wall.png", &textureWidth, &textureHeight);
    GLuint theBestWallTexture = glLoadTexture("assets/theBestWall.png", NULL, NULL);
    GLuint enemyTexture = glLoadTexture("assets/grinch.png", &enemyWidth, &enemyHeight);
    GLuint woodenMacheteTexture = glLoadTexture("assets/wgPix.png", &weaponTextureWidth, &weaponTextureHeight);
    GLuint weaponSlashTexture = glLoadTexture("assets/waponSlash.png", NULL, NULL);
    GLuint settingsButtonTexture = glLoadTexture("assets/SettingsButton.png", NULL, NULL);
    if (defaultWallTexture == 0 || theBestWallTexture == 0 || enemyTexture == 0 || woodenMacheteTexture == 0 || weaponSlashTexture == 0 || settingsButtonTexture == 0) {
        fprintf(stderr, "Failed to load one or more textures.\n");
        destroyTextures(defaultWallTexture, theBestWallTexture, enemyTexture, woodenMacheteTexture, weaponSlashTexture, settingsButtonTexture);
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mix_OpenAudio Error: %s\n", Mix_GetError());
        destroyTextures(defaultWallTexture, theBestWallTexture, enemyTexture, woodenMacheteTexture, weaponSlashTexture, settingsButtonTexture);
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Mix_Music *music = Mix_LoadMUS("music/BeepBox-Song.wav");
    if (music == NULL) {
        fprintf(stderr, "Mix_LoadMUS Error: %s\n", Mix_GetError());
        Mix_CloseAudio();
        destroyTextures(defaultWallTexture, theBestWallTexture, enemyTexture, woodenMacheteTexture, weaponSlashTexture, settingsButtonTexture);
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (Mix_PlayMusic(music, -1) < 0) { /* -1 means loop indefinitely */
        fprintf(stderr, "Mix_PlayMusic Error: %s\n", Mix_GetError());
        Mix_FreeMusic(music);
        Mix_CloseAudio();
        destroyTextures(defaultWallTexture, theBestWallTexture, enemyTexture, woodenMacheteTexture, weaponSlashTexture, settingsButtonTexture);
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Model propModel = {0};
    if (!modelLoad(&propModel, modelPath)) {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        Mix_CloseAudio();
        destroyTextures(defaultWallTexture, theBestWallTexture, enemyTexture, woodenMacheteTexture, weaponSlashTexture, settingsButtonTexture);
        glRendererShutdown();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    ModelInstance propInstance = {
        &propModel,
        3.5,
        2.8,
        0.3f
    };

    double bobAmount = 0.0;
    const double attackDuration = 0.42;
    const double attackCooldownDuration = 0.65;
    double attackTimer = 0.0;
    double attackCooldownTimer = 0.0;

    Player player;
    initPlayer(&player);

    Enemy enemy;
    initEnemy(&enemy, DEFALT_ENEMY_POS_X, DEFALT_ENEMY_POS_Y, enemyTexture, enemyWidth, enemyHeight);

    bool running = true;
    GLRenderTarget sceneTarget = {0};
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
            if (openMenu(window, settingsButtonTexture) == 1) running = false;
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
        if (sceneTarget.texture == 0 || renderWidth != sceneTarget.width
            || renderHeight != sceneTarget.height) {
            if (!glCreateRenderTarget(&sceneTarget, renderWidth, renderHeight)) {
                running = false;
                break;
            }
        }

        double oldX = player.x;
        double oldY = player.y;

        updatePlayer(&player, worldMap, forward, strafe, turn , mouseDeltaX , deltaTime);
        bool walking = hypot(player.x - oldX, player.y - oldY) > 0.0001;
        updateEnemy(&enemy, &player, worldMap, deltaTime);
        glBeginRenderTarget(&sceneTarget);
        bool sceneRendered = renderScene(&player, defaultWallTexture,
            theBestWallTexture, &enemy, textureWidth, textureHeight,
            renderWidth, renderHeight, worldMap, &propInstance);
        
        if (sceneRendered) {
            drawWeapon(walking, player.bobPhase, bobAmount, woodenMacheteTexture,
                weaponTextureWidth, weaponTextureHeight, weaponSlashTexture,
                renderWidth, renderHeight, deltaTime, attacking, attackProgress);
        }

        if (!sceneRendered) {
            running = false;
            break;
        }

        int outputWidth;
        int outputHeight;
        SDL_GL_GetDrawableSize(window, &outputWidth, &outputHeight);
        if (outputWidth <= 0 || outputHeight <= 0) {
            SDL_Delay(16);
            continue;
        }
        glPresentRenderTarget(&sceneTarget, outputWidth, outputHeight);
        glSwapWindow(window);
    }

    glDestroyRenderTarget(&sceneTarget);
    modelDestroy(&propModel);
    glDeleteTexture(defaultWallTexture);
    glDeleteTexture(theBestWallTexture);
    glDeleteTexture(enemy.texture);
    glDeleteTexture(woodenMacheteTexture);
    glDeleteTexture(weaponSlashTexture);
    glDeleteTexture(settingsButtonTexture);
    glRendererShutdown();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    Mix_HaltMusic();
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}
