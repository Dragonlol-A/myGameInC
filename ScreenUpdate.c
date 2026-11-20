#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <SDL2/SDL_image.h>
#include "ScreenUpdate.h"

const int worldMap[MAP_HEIGHT][MAP_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,1,2,0,0,1,0,0,1},
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

static const SDL_Point resolutions[] = {
    {640, 480},
    {800, 600},
    {1280, 720},
    {1600, 900},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160}
};

static int resolutionIndex = 4;
static bool fullscreenEnabled = false;
static bool vsyncEnabled = true;

typedef struct {
    char character;
    Uint8 rows[5];
} BitmapGlyph;

static const BitmapGlyph bitmapFont[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}},
    {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 7, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}},
    {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'C', {7, 4, 4, 4, 7}}, {'E', {7, 4, 6, 4, 7}},
    {'F', {7, 4, 6, 4, 4}},
    {'I', {7, 2, 2, 2, 7}}, {'L', {4, 4, 4, 4, 7}},
    {'K', {5, 5, 6, 5, 5}},
    {'N', {5, 7, 7, 7, 5}}, {'O', {7, 5, 5, 5, 7}},
    {'P', {6, 5, 6, 4, 4}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}},
    {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}},
    {'X', {5, 2, 2, 2, 5}}, {'Y', {5, 5, 2, 2, 2}}
};

static void drawBitmapText(SDL_Renderer *renderer, const char *text, int x, int y, int scale) {
    for (const char *character = text; *character != '\0'; character++) {
        const Uint8 *glyphRows = NULL;
        for (size_t glyphIndex = 0; glyphIndex < sizeof(bitmapFont) / sizeof(bitmapFont[0]); glyphIndex++) {
            if (bitmapFont[glyphIndex].character == *character) {
                glyphRows = bitmapFont[glyphIndex].rows;
                break;
            }
        }

        if (glyphRows != NULL) {
            for (int row = 0; row < 5; row++) {
                for (int column = 0; column < 3; column++) {
                    if ((glyphRows[row] & (1u << (2 - column))) != 0) {
                        SDL_Rect pixel = {x + column * scale, y + row * scale, scale, scale};
                        SDL_RenderFillRect(renderer, &pixel);
                    }
                }
            }
        }
        x += 4 * scale;
    }
}

static bool pointInRect(float x, float y, const SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w
        && y >= rect->y && y < rect->y + rect->h;
}

static void updatePausedWindowTitle(SDL_Window *window) {
    char title[64];
    if (fullscreenEnabled) {
        snprintf(title, sizeof(title), "Paused - Fullscreen");
    } else {
        snprintf(title, sizeof(title), "Paused - %dx%d",
            resolutions[resolutionIndex].x, resolutions[resolutionIndex].y);
    }
    SDL_SetWindowTitle(window, title);
}

static void setDrawColor(SDL_Renderer *renderer, Uint8 red, Uint8 green, Uint8 blue) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
}

void getRenderResolution(int *width, int *height) {
    *width = resolutions[resolutionIndex].x;
    *height = resolutions[resolutionIndex].y;
}

static int openResolutionSettings(SDL_Renderer *renderer, SDL_Window *gameWindow) {
    const int settingsWidth = 480;
    const int settingsHeight = 620;
    const int dropdownX = 32;
    const int dropdownY = 80;
    const int dropdownWidth = 416;
    const int dropdownHeight = 56;
    const int optionY = dropdownY + dropdownHeight;
    const int optionHeight = 44;
    const SDL_Rect fullscreenCheckbox = {32, 450, 24, 24};
    const SDL_Rect vsyncCheckbox = {32, 480, 24, 24};
    const SDL_Rect applyButton = {32, 540, 198, 54};
    const SDL_Rect backButton = {250, 540, 198, 54};
    const int resolutionCount = (int)(sizeof(resolutions) / sizeof(resolutions[0]));

    if (SDL_RenderSetLogicalSize(renderer, settingsWidth, settingsHeight) != 0) {
        fprintf(stderr, "SDL_RenderSetLogicalSize settings Error: %s\n", SDL_GetError());
        return 0;
    }

    bool settingsOpen = true;
    bool dropdownOpen = false;
    int quitRequested = 0;
    int originalResolutionIndex = resolutionIndex;
    bool originalFullscreenEnabled = fullscreenEnabled;
    bool originalVsyncEnabled = vsyncEnabled;
    Uint32 gameWindowId = SDL_GetWindowID(gameWindow);
    SDL_SetWindowTitle(gameWindow, "Settings - Resolution");

    while (settingsOpen) {
        setDrawColor(renderer, 22, 26, 38);
        SDL_RenderClear(renderer);

        setDrawColor(renderer, 235, 238, 245);
        drawBitmapText(renderer, "RESOLUTION", 32, 32, 5);

        SDL_Rect dropdown = {dropdownX, dropdownY, dropdownWidth, dropdownHeight};
        setDrawColor(renderer, 48, 56, 76);
        SDL_RenderFillRect(renderer, &dropdown);
        setDrawColor(renderer, 180, 190, 210);
        SDL_RenderDrawRect(renderer, &dropdown);

        char resolutionLabel[32];
        snprintf(resolutionLabel, sizeof(resolutionLabel), "%dx%d", resolutions[resolutionIndex].x, resolutions[resolutionIndex].y);
        setDrawColor(renderer, 255, 255, 255);
        drawBitmapText(renderer, resolutionLabel, dropdownX + 16, dropdownY + 18, 5);
        SDL_RenderDrawLine(renderer, dropdownX + dropdownWidth - 34, dropdownY + 23,
            dropdownX + dropdownWidth - 24, dropdownY + 33);
        SDL_RenderDrawLine(renderer, dropdownX + dropdownWidth - 24, dropdownY + 33,
            dropdownX + dropdownWidth - 14, dropdownY + 23);

        if (dropdownOpen) {
            for (int index = 0; index < resolutionCount; index++) {
                SDL_Rect option = {
                    dropdownX,
                    optionY + index * optionHeight,
                    dropdownWidth,
                    optionHeight
                };
                if (index == resolutionIndex) {
                    setDrawColor(renderer, 58, 105, 78);
                } else {
                    setDrawColor(renderer, 38, 45, 62);
                }
                SDL_RenderFillRect(renderer, &option);
                setDrawColor(renderer, 110, 125, 150);
                SDL_RenderDrawRect(renderer, &option);

                snprintf(resolutionLabel, sizeof(resolutionLabel), "%dx%d",
                    resolutions[index].x, resolutions[index].y);
                setDrawColor(renderer, 255, 255, 255);
                drawBitmapText(renderer, resolutionLabel, dropdownX + 16, option.y + 12, 4);
            }
        }

        setDrawColor(renderer, 180, 190, 210);
        SDL_RenderDrawRect(renderer, &fullscreenCheckbox);
        if (fullscreenEnabled) {
            SDL_Rect checkmark = {
                fullscreenCheckbox.x + 5,
                fullscreenCheckbox.y + 5,
                fullscreenCheckbox.w - 10,
                fullscreenCheckbox.h - 10
            };
            setDrawColor(renderer, 54, 130, 88);
            SDL_RenderFillRect(renderer, &checkmark);
        }
        setDrawColor(renderer, 255, 255, 255);
        drawBitmapText(renderer, "FULLSCREEN", 68, 452, 4);

        setDrawColor(renderer, 180, 190, 210);
        SDL_RenderDrawRect(renderer, &vsyncCheckbox);
        if (vsyncEnabled) {
            SDL_Rect checkmark = {
                vsyncCheckbox.x + 5,
                vsyncCheckbox.y + 5,
                vsyncCheckbox.w - 10,
                vsyncCheckbox.h - 10
            };
            setDrawColor(renderer, 54, 130, 88);
            SDL_RenderFillRect(renderer, &checkmark);
        }
        setDrawColor(renderer, 255, 255, 255);
        drawBitmapText(renderer, "VSYNC", 68, 482, 4);

        setDrawColor(renderer, 54, 130, 88);
        SDL_RenderFillRect(renderer, &applyButton);
        setDrawColor(renderer, 255, 255, 255);
        drawBitmapText(renderer, "APPLY", applyButton.x + 49, applyButton.y + 17, 5);

        SDL_Rect backButtonColor = backButton;
        setDrawColor(renderer, 72, 82, 105);
        SDL_RenderFillRect(renderer, &backButtonColor);
        setDrawColor(renderer, 255, 255, 255);
        drawBitmapText(renderer, "BACK", backButton.x + 59, backButton.y + 17, 5);

        SDL_RenderPresent(renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quitRequested = 1;
                settingsOpen = false;
                break;
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (event.window.windowID == gameWindowId) {
                    quitRequested = 1;
                    settingsOpen = false;
                }
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.windowID == gameWindowId && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                resolutionIndex = originalResolutionIndex;
                fullscreenEnabled = originalFullscreenEnabled;
                vsyncEnabled = originalVsyncEnabled;
                settingsOpen = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN
                && event.button.button == SDL_BUTTON_LEFT
                && event.button.windowID == gameWindowId) {
                float mouseX = (float)event.button.x;
                float mouseY = (float)event.button.y;

                if (pointInRect(mouseX, mouseY, &fullscreenCheckbox)) {
                    fullscreenEnabled = !fullscreenEnabled;
                } else if (pointInRect(mouseX, mouseY, &vsyncCheckbox)) {
                    vsyncEnabled = !vsyncEnabled;
                } else if (pointInRect(mouseX, mouseY, &dropdown)) {
                    dropdownOpen = !dropdownOpen;
                } else if (dropdownOpen) {
                    bool optionSelected = false;
                    for (int index = 0; index < resolutionCount; index++) {
                        SDL_Rect option = {
                            dropdownX,
                            optionY + index * optionHeight,
                            dropdownWidth,
                            optionHeight
                        };
                        if (pointInRect(mouseX, mouseY, &option)) {
                            resolutionIndex = index;
                            dropdownOpen = false;
                            optionSelected = true;
                            break;
                        }
                    }
                    if (!optionSelected) {
                        dropdownOpen = false;
                    }
                }

                if (pointInRect(mouseX, mouseY, &applyButton)) {
                    bool vsyncChanged = vsyncEnabled != originalVsyncEnabled;
                    if (vsyncChanged && SDL_RenderSetVSync(renderer, vsyncEnabled ? 1 : 0) != 0) {
                        fprintf(stderr, "SDL_RenderSetVSync Error: %s\n", SDL_GetError());
                        resolutionIndex = originalResolutionIndex;
                        fullscreenEnabled = originalFullscreenEnabled;
                        vsyncEnabled = originalVsyncEnabled;
                    } else {
                        int fullscreenFlags = fullscreenEnabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
                        if (SDL_SetWindowFullscreen(gameWindow, fullscreenFlags) != 0) {
                            fprintf(stderr, "SDL_SetWindowFullscreen Error: %s\n", SDL_GetError());
                            if (vsyncChanged
                                && SDL_RenderSetVSync(renderer, originalVsyncEnabled ? 1 : 0) != 0) {
                                fprintf(stderr, "SDL_RenderSetVSync rollback Error: %s\n", SDL_GetError());
                            }
                            resolutionIndex = originalResolutionIndex;
                            fullscreenEnabled = originalFullscreenEnabled;
                            vsyncEnabled = originalVsyncEnabled;
                        } else {
                            if (!fullscreenEnabled) {
                                SDL_SetWindowSize(gameWindow,
                                    resolutions[resolutionIndex].x, resolutions[resolutionIndex].y);
                                SDL_SetWindowPosition(gameWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                            }
                            originalResolutionIndex = resolutionIndex;
                            originalFullscreenEnabled = fullscreenEnabled;
                            originalVsyncEnabled = vsyncEnabled;
                            updatePausedWindowTitle(gameWindow);
                        }
                    }
                } else if (pointInRect(mouseX, mouseY, &backButton)) {
                    resolutionIndex = originalResolutionIndex;
                    fullscreenEnabled = originalFullscreenEnabled;
                    vsyncEnabled = originalVsyncEnabled;
                    settingsOpen = false;
                }
            }
        }

        SDL_Delay(16);
    }

    if (SDL_RenderSetLogicalSize(renderer, 0, 0) != 0) {
        fprintf(stderr, "SDL_RenderSetLogicalSize reset Error: %s\n", SDL_GetError());
    }
    updatePausedWindowTitle(gameWindow);
    return quitRequested;
}

SDL_Texture *loadTextures(SDL_Renderer *renderer, const char *texturePath) {
    SDL_Surface *surface = IMG_Load(texturePath);

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

static bool drawTexturedFloor(SDL_Renderer *renderer, const Player *player,
    SDL_Texture *floorTexture, int renderWidth, int renderHeight) {
    int floorHeight = renderHeight - renderHeight / 2;
    double rayDirX0 = player->dirX - player->planeX;
    double rayDirY0 = player->dirY - player->planeY;
    double rayDirX1 = player->dirX + player->planeX;
    double rayDirY1 = player->dirY + player->planeY;
    double stepHeight = 0.5 * renderHeight;
    size_t maxQuads = 0;

    for (int y = 0; y < floorHeight; y++) {
        double rowDistance = stepHeight / (y + 1);
        double worldWidthX = rowDistance * (rayDirX1 - rayDirX0);
        double worldWidthY = rowDistance * (rayDirY1 - rayDirY0);
        size_t rowQuads = (size_t)ceil(fabs(worldWidthX)) + (size_t)ceil(fabs(worldWidthY)) + 1;
        if (rowQuads > SIZE_MAX - maxQuads) {
            fprintf(stderr, "Floor geometry is too large.\n");
            return false;
        }
        maxQuads += rowQuads;
    }

    if (maxQuads > (size_t)INT_MAX / 6 || maxQuads > SIZE_MAX / (4 * sizeof(SDL_Vertex)) || maxQuads > SIZE_MAX / (6 * sizeof(int))) {
        fprintf(stderr, "Floor geometry is too large.\n");
        return false;
    }

    SDL_Vertex *vertices = malloc(maxQuads * 4 * sizeof(*vertices));
    int *indices = malloc(maxQuads * 6 * sizeof(*indices));
    if (vertices == NULL || indices == NULL) {
        fprintf(stderr, "Failed to allocate floor geometry.\n");
        free(vertices);
        free(indices);
        return false;
    }

    int vertexCount = 0;
    int indexCount = 0;

    for (int y = 0; y < floorHeight; y++) {
        double rowDistance = stepHeight / (y + 1);
        double startX = player->x + rowDistance * rayDirX0;
        double startY = player->y + rowDistance * rayDirY0;
        double deltaX = rowDistance * (rayDirX1 - rayDirX0);
        double deltaY = rowDistance * (rayDirY1 - rayDirY0);
        double nextX = INFINITY;
        double nextY = INFINITY;
        double stepTX = INFINITY;
        double stepTY = INFINITY;

        if (deltaX > 0.0) {
            nextX = (floor(startX) + 1.0 - startX) / deltaX;
            stepTX = 1.0 / deltaX;
        } else if (deltaX < 0.0) {
            nextX = (ceil(startX) - 1.0 - startX) / deltaX;
            stepTX = -1.0 / deltaX;
        }
        if (deltaY > 0.0) {
            nextY = (floor(startY) + 1.0 - startY) / deltaY;
            stepTY = 1.0 / deltaY;
        } else if (deltaY < 0.0) {
            nextY = (ceil(startY) - 1.0 - startY) / deltaY;
            stepTY = -1.0 / deltaY;
        }

        double currentT = 0.0;
        while (currentT < 1.0) {
            double endT = fmin(1.0, fmin(nextX, nextY));
            double middleT = (currentT + endT) * 0.5;
            double tileX = floor(startX + deltaX * middleT);
            double tileY = floor(startY + deltaY * middleT);
            double u0 = startX + deltaX * currentT - tileX;
            double v0 = startY + deltaY * currentT - tileY;
            double u1 = startX + deltaX * endT - tileX;
            double v1 = startY + deltaY * endT - tileY;
            float textureU0 = (float)fmax(0.0, fmin(1.0, u0));
            float textureV0 = (float)fmax(0.0, fmin(1.0, v0));
            float textureU1 = (float)fmax(0.0, fmin(1.0, u1));
            float textureV1 = (float)fmax(0.0, fmin(1.0, v1));

            SDL_Color white = {255, 255, 255, 255};
            float screenX0 = (float)(currentT * renderWidth);
            float screenX1 = (float)(endT * renderWidth);
            float screenY0 = (float)(renderHeight / 2 + y);
            float screenY1 = screenY0 + 1.0f;
            int baseVertex = vertexCount;

            vertices[vertexCount++] = (SDL_Vertex){
                {screenX0, screenY0}, white, {textureU0, textureV0}
            };
            vertices[vertexCount++] = (SDL_Vertex){
                {screenX1, screenY0}, white, {textureU1, textureV1}
            };
            vertices[vertexCount++] = (SDL_Vertex){
                {screenX1, screenY1}, white, {textureU1, textureV1}
            };
            vertices[vertexCount++] = (SDL_Vertex){
                {screenX0, screenY1}, white, {textureU0, textureV0}
            };
            indices[indexCount++] = baseVertex;
            indices[indexCount++] = baseVertex + 1;
            indices[indexCount++] = baseVertex + 2;
            indices[indexCount++] = baseVertex;
            indices[indexCount++] = baseVertex + 2;
            indices[indexCount++] = baseVertex + 3;

            if (endT >= 1.0) {
                break;
            }
            currentT = endT;
            if (nextX <= endT) {
                nextX += stepTX;
            }
            if (nextY <= endT) {
                nextY += stepTY;
            }
        }
    }

    SDL_BlendMode previousBlendMode;
    if (SDL_GetTextureBlendMode(floorTexture, &previousBlendMode) != 0) {
        fprintf(stderr, "SDL_GetTextureBlendMode floor Error: %s\n", SDL_GetError());
        free(vertices);
        free(indices);
        return false;
    }
    if (SDL_SetTextureBlendMode(floorTexture, SDL_BLENDMODE_NONE) != 0
        || SDL_SetTextureColorMod(floorTexture, 255, 255, 255) != 0) {
        fprintf(stderr, "Failed to configure floor texture: %s\n", SDL_GetError());
        SDL_SetTextureBlendMode(floorTexture, previousBlendMode);
        free(vertices);
        free(indices);
        return false;
    }

    bool rendered = SDL_RenderGeometry(renderer, floorTexture, vertices,
        vertexCount, indices, indexCount) == 0;
    if (!rendered) {
        fprintf(stderr, "SDL_RenderGeometry floor Error: %s\n", SDL_GetError());
    }
    bool restored = SDL_SetTextureBlendMode(floorTexture, previousBlendMode) == 0;
    if (!restored) {
        fprintf(stderr, "SDL_SetTextureBlendMode restore Error: %s\n", SDL_GetError());
    }

    free(vertices);
    free(indices);
    return rendered && restored;
}

bool renderScene(SDL_Renderer *renderer, const Player *player, SDL_Texture *wallTexture, SDL_Texture *theBestWallTexture, const Enemy *enemy, int textureWidth, int textureHeight, int renderWidth, int renderHeight, const int map[MAP_HEIGHT][MAP_WIDTH]) {
    if (renderWidth <= 0 || renderHeight <= 0) {
        fprintf(stderr, "Renderer output size is invalid: %d x %d\n", renderWidth, renderHeight);
        return false;
    }

    double *zBuffer = malloc((size_t)renderWidth * sizeof(*zBuffer));
    if (zBuffer == NULL) {
        fprintf(stderr, "Failed to allocate ray z-buffer.\n");
        return false;
    }

    setDrawColor(renderer, 42, 48, 68);
    SDL_Rect ceiling = {0, 0, renderWidth, renderHeight / 2};
    SDL_RenderFillRect(renderer, &ceiling);

    setDrawColor(renderer, 62, 54, 45);
    SDL_Rect floorRect = {0, renderHeight / 2, renderWidth, renderHeight - renderHeight / 2};
    SDL_RenderFillRect(renderer, &floorRect);
    if (!drawTexturedFloor(renderer, player, wallTexture, renderWidth, renderHeight)) {
        free(zBuffer);
        return false;
    }

    for (int x = 0; x < renderWidth; x++) {
        zBuffer[x] = INFINITY;
        double cameraX = 2.0 * x / renderWidth - 1.0;
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

        zBuffer[x] = perpendicularDistance;

        double wallX;
        if (side == 0) {
            wallX = player->y + perpendicularDistance * rayDirY;
        } else {
            wallX = player->x + perpendicularDistance * rayDirX;
        }
        wallX -= floor(wallX);

        int textureX = (int)(wallX * textureWidth);
        if (textureX < 0) {
            textureX = 0;
        } else if (textureX >= textureWidth) {
            textureX = textureWidth - 1;
        }

        int lineHeight = (int)(renderHeight / perpendicularDistance);
        if (lineHeight < 1) {
            lineHeight = 1;
        }

        int projectedStart = renderHeight / 2 - lineHeight / 2;
        int projectedEnd = projectedStart + lineHeight;
        int drawStart = projectedStart < 0 ? 0 : projectedStart;
        int drawEnd = projectedEnd > renderHeight ? renderHeight : projectedEnd;
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

        if (map[mapY][mapX] == 1) {
            SDL_SetTextureColorMod(wallTexture, shade, shade, shade);
            SDL_Rect source = {textureX, sourceStart, 1, sourceEnd - sourceStart};
            SDL_Rect destination = {x, drawStart, 1, drawEnd - drawStart};
            SDL_RenderCopy(renderer, wallTexture, &source, &destination);
        }
        if(map[mapY][mapX] == 2) {
            SDL_SetTextureColorMod(theBestWallTexture, shade, shade, shade);
            SDL_Rect source = {textureX, sourceStart, 1, sourceEnd - sourceStart};
            SDL_Rect destination = {x, drawStart, 1, drawEnd - drawStart};
            SDL_RenderCopy(renderer, theBestWallTexture, &source, &destination);
        }
    }
    double relativeX = enemy->x - player->x;
double relativeY = enemy->y - player->y;

double determinant =
    player->planeX * player->dirY - player->dirX * player->planeY;
double inverseDeterminant = 1.0 / determinant;

double transformX = inverseDeterminant *
    (player->dirY * relativeX - player->dirX * relativeY);
double transformY = inverseDeterminant *
    (-player->planeY * relativeX + player->planeX * relativeY);

if (transformY > 0.1) {
    int screenX = (int)((renderWidth / 2.0) * (1.0 + transformX / transformY));
    double spriteDistance = hypot(relativeX, relativeY);
    int spriteHeight = (int)(renderHeight / spriteDistance);
    int spriteWidth = (int)(spriteHeight * (double)enemy->textureWidth / enemy->textureHeight);

    int spriteLeft = screenX - spriteWidth / 2;
    int spriteTop = renderHeight / 2 - spriteHeight / 2;

    int firstX = spriteLeft < 0 ? 0 : spriteLeft;
    int endX = spriteLeft + spriteWidth;
    if (endX > renderWidth) {
        endX = renderWidth;
    }

    SDL_SetTextureBlendMode(enemy->texture, SDL_BLENDMODE_BLEND);

    for (int stripeX = firstX; stripeX < endX; stripeX++) {
        if (transformY < zBuffer[stripeX]) {
            int sourceX = (int)((stripeX - spriteLeft) *
                (double)enemy->textureWidth / spriteWidth);

            SDL_Rect source = {sourceX, 0, 1, enemy->textureHeight};
            SDL_Rect destination = {stripeX, spriteTop, 1, spriteHeight};
            SDL_RenderCopy(renderer, enemy->texture, &source, &destination);
        }
    }
}

    free(zBuffer);
    SDL_SetTextureColorMod(wallTexture, 255, 255, 255);
    return true;
}

int openMenu(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture *settingsButtonTexture) {
    bool onMenu = true;

    SDL_SetRelativeMouseMode(SDL_FALSE);
    updatePausedWindowTitle(window);
    Uint32 windowId = SDL_GetWindowID(window);

    while (onMenu) {
        int renderWidth;
        int renderHeight;
        if (SDL_GetRendererOutputSize(renderer, &renderWidth, &renderHeight) != 0) {
            fprintf(stderr, "SDL_GetRendererOutputSize Error: %s\n", SDL_GetError());
            SDL_SetRelativeMouseMode(SDL_TRUE);
            return 1;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        int buttonSizeH = renderHeight / 8;
        int buttonSizeW= renderWidth /8;

        SDL_Rect optionButton = { renderWidth / 2 - buttonSizeW , renderHeight / 2 - buttonSizeH / 2, buttonSizeW *2, buttonSizeH};

        SDL_RenderCopy(renderer, settingsButtonTexture, NULL, &optionButton);
        SDL_RenderPresent(renderer);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                return 1;
            }

            if (event.type == SDL_WINDOWEVENT && event.window.windowID == windowId
                && event.window.event == SDL_WINDOWEVENT_CLOSE) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                return 1;
            }

            if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.windowID == windowId) {
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    SDL_SetWindowTitle(window, "Raycasting Demo");
                    onMenu = false;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN
                && event.button.button == SDL_BUTTON_LEFT
                && event.button.windowID == windowId) {
                float mouseX;
                float mouseY;
                SDL_RenderWindowToLogical(renderer, event.button.x, event.button.y, &mouseX, &mouseY);
                if (pointInRect(mouseX, mouseY, &optionButton)
                    && openResolutionSettings(renderer, window) != 0) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    return 1;
                }
            }
        }
        SDL_Delay(100);
    }

    return 0;
}

