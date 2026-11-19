#include <SDL2/SDL.h>
#include <stdio.h>

typedef struct{
    double x;
    double y;
} Vector2;

typedef struct {
    double z;
    double radius;
    Vector2 vector;
    SDL_Texture *texture;
} Object;
