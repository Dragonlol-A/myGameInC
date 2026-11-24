#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static SDL_Color currentDrawColor;

static void fillRect(const SDL_Rect *rect) {
    glFillRect((float)rect->x, (float)rect->y,
        (float)rect->w, (float)rect->h, currentDrawColor);
}

static void outlineRect(const SDL_Rect *rect) {
    glOutlineRect((float)rect->x, (float)rect->y,
        (float)rect->w, (float)rect->h, currentDrawColor);
}

static void drawBitmapText(const char *text, int x, int y, int scale) {
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
                        glFillRect((float)(x + column * scale),
                            (float)(y + row * scale), (float)scale, (float)scale,
                            currentDrawColor);
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

static void windowPointToLogical(SDL_Window *window, int windowX, int windowY,
    int logicalWidth, int logicalHeight, float *logicalX, float *logicalY) {
    int windowWidth;
    int windowHeight;
    int drawableWidth;
    int drawableHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    if (windowWidth <= 0 || windowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0 || logicalWidth <= 0 || logicalHeight <= 0) {
        *logicalX = -1.0f;
        *logicalY = -1.0f;
        return;
    }

    float scale = fminf((float)drawableWidth / logicalWidth, (float)drawableHeight / logicalHeight);
    float viewportWidth = (float)(int)(logicalWidth * scale);
    float viewportHeight = (float)(int)(logicalHeight * scale);
    float offsetX = (drawableWidth - viewportWidth) * 0.5f;
    float offsetY = (drawableHeight - viewportHeight) * 0.5f;
    float drawableX = (float)windowX * drawableWidth / windowWidth;
    float drawableY = (float)windowY * drawableHeight / windowHeight;
    float scaleX = viewportWidth / logicalWidth;
    float scaleY = viewportHeight / logicalHeight;
    *logicalX = (drawableX - offsetX) / scaleX;
    *logicalY = (drawableY - offsetY) / scaleY;
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

static void setDrawColor(Uint8 red, Uint8 green, Uint8 blue) {
    currentDrawColor = (SDL_Color){red, green, blue, 255};
}

void getRenderResolution(int *width, int *height) {
    *width = resolutions[resolutionIndex].x;
    *height = resolutions[resolutionIndex].y;
}

static int openResolutionSettings(SDL_Window *gameWindow) {
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

    bool settingsOpen = true;
    bool dropdownOpen = false;
    int quitRequested = 0;
    int originalResolutionIndex = resolutionIndex;
    bool originalFullscreenEnabled = fullscreenEnabled;
    bool originalVsyncEnabled = vsyncEnabled;
    Uint32 gameWindowId = SDL_GetWindowID(gameWindow);
    SDL_SetWindowTitle(gameWindow, "Settings - Resolution");

    while (settingsOpen) {
        int drawableWidth;
        int drawableHeight;
        SDL_GL_GetDrawableSize(gameWindow, &drawableWidth, &drawableHeight);
        glSetLogicalProjection(drawableWidth, drawableHeight, settingsWidth, settingsHeight);
        glRendererClear(22, 26, 38);

        setDrawColor(235, 238, 245);
        drawBitmapText("RESOLUTION", 32, 32, 5);

        SDL_Rect dropdown = {dropdownX, dropdownY, dropdownWidth, dropdownHeight};
        setDrawColor(48, 56, 76);
        fillRect(&dropdown);
        setDrawColor(180, 190, 210);
        outlineRect(&dropdown);

        char resolutionLabel[32];
        snprintf(resolutionLabel, sizeof(resolutionLabel), "%dx%d",
            resolutions[resolutionIndex].x, resolutions[resolutionIndex].y);
        setDrawColor(255, 255, 255);
        drawBitmapText(resolutionLabel, dropdownX + 16, dropdownY + 18, 5);
        glDrawLine(dropdownX + dropdownWidth - 34, dropdownY + 23,
            dropdownX + dropdownWidth - 24, dropdownY + 33, currentDrawColor);
        glDrawLine(dropdownX + dropdownWidth - 24, dropdownY + 33,
            dropdownX + dropdownWidth - 14, dropdownY + 23, currentDrawColor);

        if (dropdownOpen) {
            for (int index = 0; index < resolutionCount; index++) {
                SDL_Rect option = {
                    dropdownX,
                    optionY + index * optionHeight,
                    dropdownWidth,
                    optionHeight
                };
                if (index == resolutionIndex) {
                    setDrawColor(58, 105, 78);
                } else {
                    setDrawColor(38, 45, 62);
                }
                fillRect(&option);
                setDrawColor(110, 125, 150);
                outlineRect(&option);

                snprintf(resolutionLabel, sizeof(resolutionLabel), "%dx%d",
                    resolutions[index].x, resolutions[index].y);
                setDrawColor(255, 255, 255);
                drawBitmapText(resolutionLabel, dropdownX + 16, option.y + 12, 4);
            }
        }

        setDrawColor(180, 190, 210);
        outlineRect(&fullscreenCheckbox);
        if (fullscreenEnabled) {
            SDL_Rect checkmark = {
                fullscreenCheckbox.x + 5,
                fullscreenCheckbox.y + 5,
                fullscreenCheckbox.w - 10,
                fullscreenCheckbox.h - 10
            };
            setDrawColor(54, 130, 88);
            fillRect(&checkmark);
        }
        setDrawColor(255, 255, 255);
        drawBitmapText("FULLSCREEN", 68, 452, 4);

        setDrawColor(180, 190, 210);
        outlineRect(&vsyncCheckbox);
        if (vsyncEnabled) {
            SDL_Rect checkmark = {
                vsyncCheckbox.x + 5,
                vsyncCheckbox.y + 5,
                vsyncCheckbox.w - 10,
                vsyncCheckbox.h - 10
            };
            setDrawColor(54, 130, 88);
            fillRect(&checkmark);
        }
        setDrawColor(255, 255, 255);
        drawBitmapText("VSYNC", 68, 482, 4);

        setDrawColor(54, 130, 88);
        fillRect(&applyButton);
        setDrawColor(255, 255, 255);
        drawBitmapText("APPLY", applyButton.x + 49, applyButton.y + 17, 5);

        SDL_Rect backButtonColor = backButton;
        setDrawColor(72, 82, 105);
        fillRect(&backButtonColor);
        setDrawColor(255, 255, 255);
        drawBitmapText("BACK", backButton.x + 59, backButton.y + 17, 5);

        SDL_GL_SwapWindow(gameWindow);

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
                float mouseX;
                float mouseY;
                windowPointToLogical(gameWindow, event.button.x, event.button.y,
                    settingsWidth, settingsHeight, &mouseX, &mouseY);

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
                    if (vsyncChanged && glSetSwapInterval(vsyncEnabled) != 0) {
                        fprintf(stderr, "SDL_GL_SetSwapInterval Error: %s\n", SDL_GetError());
                        resolutionIndex = originalResolutionIndex;
                        fullscreenEnabled = originalFullscreenEnabled;
                        vsyncEnabled = originalVsyncEnabled;
                    } else {
                        int fullscreenFlags = fullscreenEnabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
                        if (SDL_SetWindowFullscreen(gameWindow, fullscreenFlags) != 0) {
                            fprintf(stderr, "SDL_SetWindowFullscreen Error: %s\n", SDL_GetError());
                            if (vsyncChanged
                                && glSetSwapInterval(originalVsyncEnabled) != 0) {
                                fprintf(stderr, "SDL_GL_SetSwapInterval rollback Error: %s\n", SDL_GetError());
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

    int drawableWidth;
    int drawableHeight;
    SDL_GL_GetDrawableSize(gameWindow, &drawableWidth, &drawableHeight);
    glSet2DProjection(drawableWidth, drawableHeight);
    updatePausedWindowTitle(gameWindow);
    return quitRequested;
}

void destroyTextures(GLuint defaultWallTexture, GLuint theBestWallTexture, GLuint enemyTexture, GLuint woodenMacheteTexture, GLuint weaponSlashTexture, GLuint settingsButtonTexture) {
        glDeleteTexture(defaultWallTexture);
        glDeleteTexture(theBestWallTexture);
        glDeleteTexture(enemyTexture);
        glDeleteTexture(woodenMacheteTexture);
        glDeleteTexture(weaponSlashTexture);
        glDeleteTexture(settingsButtonTexture);
        glRendererShutdown();
        return;
}

typedef struct {
    int x;
    int drawStart;
    int drawEnd;
    int textureX;
    int sourceStart;
    int sourceEnd;
    Uint8 shade;
    int material;
} WallColumn;

bool renderScene(const Player *player, GLuint wallTexture, GLuint theBestWallTexture,
    const Enemy *enemy, int textureWidth, int textureHeight, int renderWidth,
    int renderHeight, const int map[MAP_HEIGHT][MAP_WIDTH]) {
    if (renderWidth <= 0 || renderHeight <= 0) {
        fprintf(stderr, "Renderer output size is invalid: %d x %d\n", renderWidth, renderHeight);
        return false;
    }

    double *zBuffer = malloc((size_t)renderWidth * sizeof(*zBuffer));
    WallColumn *wallColumns = calloc((size_t)renderWidth, sizeof(*wallColumns));
    if (zBuffer == NULL || wallColumns == NULL) {
        fprintf(stderr, "Failed to allocate raycasting buffers.\n");
        free(zBuffer);
        free(wallColumns);
        return false;
    }

    double cameraOffsetY = player->cameraBob * renderHeight;
    glDrawFloor(wallTexture, player->x, player->y, player->dirX, player->dirY,
        player->planeX, player->planeY, cameraOffsetY, renderWidth, renderHeight);

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

        int projectedStart = (int)(renderHeight / 2 - lineHeight / 2 + cameraOffsetY);
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

        if (map[mapY][mapX] == 1 || map[mapY][mapX] == 2) {
            wallColumns[x] = (WallColumn){
                x, drawStart, drawEnd, textureX, sourceStart, sourceEnd,
                shade, map[mapY][mapX]
            };
        }
    }

    glEnable(GL_TEXTURE_2D);
    for (int material = 1; material <= 2; material++) {
        GLuint texture = material == 1 ? wallTexture : theBestWallTexture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glBegin(GL_QUADS);
        for (int x = 0; x < renderWidth; x++) {
            const WallColumn *column = &wallColumns[x];
            if (column->material != material) {
                continue;
            }

            float textureU = (column->textureX + 0.5f) / textureWidth;
            float textureV0 = (float)column->sourceStart / textureHeight;
            float textureV1 = (float)column->sourceEnd / textureHeight;
            glColor4ub(column->shade, column->shade, column->shade, 255);
            glTexCoord2f(textureU, textureV0);
            glVertex2f((float)column->x, (float)column->drawStart);
            glTexCoord2f(textureU, textureV0);
            glVertex2f((float)(column->x + 1), (float)column->drawStart);
            glTexCoord2f(textureU, textureV1);
            glVertex2f((float)(column->x + 1), (float)column->drawEnd);
            glTexCoord2f(textureU, textureV1);
            glVertex2f((float)column->x, (float)column->drawEnd);
        }
        glEnd();
    }

    double relativeX = enemy->x - player->x;
    double relativeY = enemy->y - player->y;
    double determinant = player->planeX * player->dirY
        - player->dirX * player->planeY;
    double inverseDeterminant = 1.0 / determinant;
    double transformX = inverseDeterminant
        * (player->dirY * relativeX - player->dirX * relativeY);
    double transformY = inverseDeterminant
        * (-player->planeY * relativeX + player->planeX * relativeY);

    if (transformY > 0.1) {
        int screenX = (int)((renderWidth / 2.0) * (1.0 + transformX / transformY));
        double spriteDistance = hypot(relativeX, relativeY);
        int spriteHeight = (int)(renderHeight / spriteDistance);
        int spriteWidth = (int)(spriteHeight
            * (double)enemy->textureWidth / enemy->textureHeight);
        if (spriteWidth > 0 && spriteHeight > 0) {
            int spriteLeft = screenX - spriteWidth / 2;
            int spriteTop = (int)(renderHeight / 2 - spriteHeight / 2 + cameraOffsetY);
            int firstX = spriteLeft < 0 ? 0 : spriteLeft;
            int endX = spriteLeft + spriteWidth;
            if (endX > renderWidth) {
                endX = renderWidth;
            }

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, enemy->texture);
            glColor4ub(255, 255, 255, 255);
            glBegin(GL_QUADS);
            for (int stripeX = firstX; stripeX < endX; stripeX++) {
                if (transformY < zBuffer[stripeX]) {
                    float textureX0 = (float)(stripeX - spriteLeft) / spriteWidth;
                    float textureX1 = (float)(stripeX + 1 - spriteLeft) / spriteWidth;
                    glTexCoord2f(textureX0, 0.0f);
                    glVertex2f((float)stripeX, (float)spriteTop);
                    glTexCoord2f(textureX1, 0.0f);
                    glVertex2f((float)(stripeX + 1), (float)spriteTop);
                    glTexCoord2f(textureX1, 1.0f);
                    glVertex2f((float)(stripeX + 1), (float)(spriteTop + spriteHeight));
                    glTexCoord2f(textureX0, 1.0f);
                    glVertex2f((float)stripeX, (float)(spriteTop + spriteHeight));
                }
            }
            glEnd();
        }
    }

    free(zBuffer);
    free(wallColumns);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(255, 255, 255, 255);
    return true;
}

void drawWeapon(bool walking, double bobPhase, double bobAmount,
    GLuint weaponTexture, int weaponTextureWidth, int weaponTextureHeight,
    GLuint slashTexture, int renderWidth, int renderHeight, double deltaTime,
    bool attacking, double attackProgress) {
    double targetAmount = walking ? 1.0 : 0.0;
    bobAmount += (targetAmount - bobAmount) * fmin(1.0, deltaTime * 8.0);
    double scale = (double)renderHeight / SCREEN_HEIGHT;
    int bobX = (int)(cos(bobPhase) * 125.0 * bobAmount * scale);
    int bobY = (int)(sin(bobPhase) * 125.0 * bobAmount * scale);
    int weaponWidth = (int)(weaponTextureWidth * scale);
    int weaponHeight = (int)(weaponTextureHeight * scale);
    float weaponX = (float)(3 * renderWidth / 5 - weaponWidth / 3 + bobX);
    float weaponY = (float)(renderHeight / 4 - weaponHeight / 5 + bobY);

    double swingAngle = 0.0;
    double progressArc = 0.0;
    if (attacking) {
        double progress = fmin(1.0, fmax(0.0, attackProgress));
        if (progress < 0.2) {
            double anticipation = progress / 0.2;
            anticipation = anticipation * anticipation * (3.0 - 2.0 * anticipation);
            swingAngle = 50.0 * anticipation;
        } else if (progress < 0.75) {
            double slashProgress = (progress - 0.2) / 0.55;
            slashProgress = slashProgress * slashProgress * (3.0 - 2.0 * slashProgress);
            swingAngle = 50.0 - 110.0 * slashProgress;
        } else {
            double recovery = (progress - 0.75) / 0.25;
            recovery = recovery * recovery * (3.0 - 2.0 * recovery);
            swingAngle = -60.0 * (1.0 - recovery);
        }

        progressArc = sin(progress * 3.14159265358979323846);
        weaponX -= (float)(50.0 * scale * progressArc);
        weaponY -= (float)(90.0 * scale * progressArc);
    }

    glDrawTexture(weaponTexture, weaponX, weaponY, (float)weaponWidth,
        (float)weaponHeight, (float)swingAngle, weaponWidth / 2.0f,
        weaponHeight * 0.9f, 255);

    if (attacking && attackProgress >= 0.4 && attackProgress <= 0.85) {
        double slashProgress = (attackProgress - 0.2) / 0.55;
        double fade = fmin(slashProgress / 0.2, (1.0 - slashProgress) / 0.2);
        fade = fmax(0.0, fmin(1.0, fade));
        int slashSize = (int)(renderHeight * 0.78);
        float slashX = (float)(renderWidth / 2 - slashSize / 2
            - renderWidth * 0.04 * progressArc);
        float slashY = (float)(renderHeight / 2 - slashSize / 2
            - renderHeight * 0.08 * progressArc);
        glDrawTexture(slashTexture, slashX, slashY, (float)slashSize,
            (float)slashSize, (float)(-45.0 + 90.0 * slashProgress),
            slashSize / 2.0f, slashSize / 2.0f, (Uint8)(220.0 * fade));
    }
}

int openMenu(SDL_Window *window, GLuint settingsButtonTexture) {
    bool onMenu = true;

    SDL_SetRelativeMouseMode(SDL_FALSE);
    updatePausedWindowTitle(window);
    Uint32 windowId = SDL_GetWindowID(window);

    while (onMenu) {
        int renderWidth;
        int renderHeight;
        SDL_GL_GetDrawableSize(window, &renderWidth, &renderHeight);
        glSet2DProjection(renderWidth, renderHeight);
        glRendererClear(0, 0, 0);
        int buttonSizeH = renderHeight / 8;
        int buttonSizeW = renderWidth / 8;

        SDL_Rect optionButton = {
            renderWidth / 2 - buttonSizeW,
            renderHeight / 2 - buttonSizeH / 2,
            buttonSizeW * 2,
            buttonSizeH
        };
        glDrawTexture(settingsButtonTexture, (float)optionButton.x,
            (float)optionButton.y, (float)optionButton.w, (float)optionButton.h,
            0.0f, (float)optionButton.w / 2.0f,
            (float)optionButton.h / 2.0f, 255);
        SDL_GL_SwapWindow(window);

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
                int windowWidth;
                int windowHeight;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                mouseX = windowWidth > 0
                    ? (float)event.button.x * renderWidth / windowWidth : 0.0f;
                mouseY = windowHeight > 0
                    ? (float)event.button.y * renderHeight / windowHeight : 0.0f;
                if (pointInRect(mouseX, mouseY, &optionButton)
                    && openResolutionSettings(window) != 0) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    return 1;
                }
            }
        }
        SDL_Delay(100);
    }

    return 0;
}

