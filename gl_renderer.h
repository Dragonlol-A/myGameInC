#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

typedef struct {
    GLuint framebuffer;
    GLuint texture;
    int width;
    int height;
} GLRenderTarget;

bool glRendererInitialize(void);
void glRendererShutdown(void);
GLuint glLoadTexture(const char *path, int *width, int *height);
void glDeleteTexture(GLuint texture);
bool glCreateRenderTarget(GLRenderTarget *target, int width, int height);
void glDestroyRenderTarget(GLRenderTarget *target);
void glBeginRenderTarget(const GLRenderTarget *target);
void glDrawFloor(GLuint texture, double playerX, double playerY,
    double directionX, double directionY, double planeX, double planeY,
    double cameraOffsetY, int width, int height);
void glPresentRenderTarget(const GLRenderTarget *target, int outputWidth, int outputHeight);
void glSet2DProjection(int width, int height);
void glSetLogicalProjection(int viewportWidth, int viewportHeight,
    int logicalWidth, int logicalHeight);
void glRendererClear(Uint8 red, Uint8 green, Uint8 blue);
void glFillRect(float x, float y, float width, float height, SDL_Color color);
void glOutlineRect(float x, float y, float width, float height, SDL_Color color);
void glDrawLine(float x1, float y1, float x2, float y2, SDL_Color color);
void glDrawTexture(GLuint texture, float x, float y, float width, float height,
    float angleDegrees, float pivotX, float pivotY, Uint8 alpha);
void glSwapWindow(SDL_Window *window);
int glSetSwapInterval(bool enabled);

#endif
