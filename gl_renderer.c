#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL2/SDL_image.h>
#include "gl_renderer.h"

typedef GLuint (APIENTRYP GLCreateShaderProc)(GLenum type);
typedef void (APIENTRYP GLShaderSourceProc)(GLuint shader, GLsizei count,
    const GLchar *const *string, const GLint *length);
typedef void (APIENTRYP GLCompileShaderProc)(GLuint shader);
typedef void (APIENTRYP GLGetShaderivProc)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP GLGetShaderInfoLogProc)(GLuint shader, GLsizei maxLength,
    GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP GLDeleteShaderProc)(GLuint shader);
typedef GLuint (APIENTRYP GLCreateProgramProc)(void);
typedef void (APIENTRYP GLAttachShaderProc)(GLuint program, GLuint shader);
typedef void (APIENTRYP GLLinkProgramProc)(GLuint program);
typedef void (APIENTRYP GLGetProgramivProc)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP GLGetProgramInfoLogProc)(GLuint program, GLsizei maxLength,
    GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP GLDeleteProgramProc)(GLuint program);
typedef void (APIENTRYP GLUseProgramProc)(GLuint program);
typedef GLint (APIENTRYP GLGetUniformLocationProc)(GLuint program, const GLchar *name);
typedef void (APIENTRYP GLUniform1iProc)(GLint location, GLint value);
typedef void (APIENTRYP GLUniform1fProc)(GLint location, GLfloat value);
typedef void (APIENTRYP GLUniform2fProc)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP GLGenFramebuffersProc)(GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP GLDeleteFramebuffersProc)(GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP GLBindFramebufferProc)(GLenum target, GLuint framebuffer);
typedef void (APIENTRYP GLFramebufferTexture2DProc)(GLenum target, GLenum attachment,
    GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRYP GLCheckFramebufferStatusProc)(GLenum target);
typedef void (APIENTRYP GLGenRenderbuffersProc)(GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP GLDeleteRenderbuffersProc)(GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRYP GLBindRenderbufferProc)(GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP GLRenderbufferStorageProc)(GLenum target, GLenum internalformat,
    GLsizei width, GLsizei height);
typedef void (APIENTRYP GLFramebufferRenderbufferProc)(GLenum target, GLenum attachment,
    GLenum renderbufferTarget, GLuint renderbuffer);

static GLCreateShaderProc pglCreateShader;
static GLShaderSourceProc pglShaderSource;
static GLCompileShaderProc pglCompileShader;
static GLGetShaderivProc pglGetShaderiv;
static GLGetShaderInfoLogProc pglGetShaderInfoLog;
static GLDeleteShaderProc pglDeleteShader;
static GLCreateProgramProc pglCreateProgram;
static GLAttachShaderProc pglAttachShader;
static GLLinkProgramProc pglLinkProgram;
static GLGetProgramivProc pglGetProgramiv;
static GLGetProgramInfoLogProc pglGetProgramInfoLog;
static GLDeleteProgramProc pglDeleteProgram;
static GLUseProgramProc pglUseProgram;
static GLGetUniformLocationProc pglGetUniformLocation;
static GLUniform1iProc pglUniform1i;
static GLUniform1fProc pglUniform1f;
static GLUniform2fProc pglUniform2f;
static GLGenFramebuffersProc pglGenFramebuffers;
static GLDeleteFramebuffersProc pglDeleteFramebuffers;
static GLBindFramebufferProc pglBindFramebuffer;
static GLFramebufferTexture2DProc pglFramebufferTexture2D;
static GLCheckFramebufferStatusProc pglCheckFramebufferStatus;
static GLGenRenderbuffersProc pglGenRenderbuffers;
static GLDeleteRenderbuffersProc pglDeleteRenderbuffers;
static GLBindRenderbufferProc pglBindRenderbuffer;
static GLRenderbufferStorageProc pglRenderbufferStorage;
static GLFramebufferRenderbufferProc pglFramebufferRenderbuffer;

static GLuint floorProgram;
static GLint floorTextureUniform;
static GLint floorPositionUniform;
static GLint floorDirectionUniform;
static GLint floorPlaneUniform;
static GLint floorCameraOffsetUniform;
static GLint floorResolutionUniform;

static bool loadGLFunction(void *destination, size_t destinationSize, const char *name) {
    void *function = SDL_GL_GetProcAddress(name);
    if (function == NULL || destinationSize != sizeof(function)) {
        fprintf(stderr, "OpenGL function not available: %s\n", name);
        return false;
    }
    memcpy(destination, &function, destinationSize);
    return true;
}

static bool loadGLFunctionAlias(void *destination, size_t destinationSize,
    const char *name, const char *fallbackName) {
    void *function = SDL_GL_GetProcAddress(name);
    if (function == NULL && fallbackName != NULL) {
        function = SDL_GL_GetProcAddress(fallbackName);
    }
    if (function == NULL || destinationSize != sizeof(function)) {
        fprintf(stderr, "OpenGL function not available: %s\n", name);
        return false;
    }
    memcpy(destination, &function, destinationSize);
    return true;
}

#define LOAD_GL_FUNCTION(variable, name) \
    loadGLFunction(&(variable), sizeof(variable), (name))
#define LOAD_GL_FUNCTION_ALIAS(variable, name, fallback) \
    loadGLFunctionAlias(&(variable), sizeof(variable), (name), (fallback))

static GLuint compileFloorProgram(void) {
    static const GLchar *fragmentSource =
        "#version 120\n"
        "uniform sampler2D floorTexture;\n"
        "uniform vec2 playerPosition;\n"
        "uniform vec2 direction;\n"
        "uniform vec2 cameraPlane;\n"
        "uniform float cameraOffsetY;\n"
        "uniform vec2 resolution;\n"
        "void main() {\n"
        "    float horizon = resolution.y * 0.5 - cameraOffsetY;\n"
        "    float row = horizon - gl_FragCoord.y;\n"
        "    if (row <= 0.0) discard;\n"
        "    float distance = (resolution.y * 0.5) / row;\n"
        "    float cameraX = 2.0 * gl_FragCoord.x / resolution.x - 1.0;\n"
        "    vec2 ray = direction + cameraPlane * cameraX;\n"
        "    vec2 worldPosition = playerPosition + ray * distance;\n"
        "    gl_FragColor = vec4(texture2D(floorTexture, worldPosition).rgb, 1.0);\n"
        "}\n";
    GLuint shader = pglCreateShader(GL_FRAGMENT_SHADER);
    if (shader == 0) {
        fprintf(stderr, "glCreateShader floor Error\n");
        return 0;
    }
    pglShaderSource(shader, 1, &fragmentSource, NULL);
    pglCompileShader(shader);

    GLint compiled = GL_FALSE;
    pglGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLchar log[2048];
        GLsizei logLength = 0;
        pglGetShaderInfoLog(shader, (GLsizei)sizeof(log), &logLength, log);
        fprintf(stderr, "Floor shader compilation failed: %.*s\n",
            (int)logLength, log);
        pglDeleteShader(shader);
        return 0;
    }

    GLuint program = pglCreateProgram();
    if (program == 0) {
        fprintf(stderr, "glCreateProgram floor Error\n");
        pglDeleteShader(shader);
        return 0;
    }
    pglAttachShader(program, shader);
    pglLinkProgram(program);
    pglDeleteShader(shader);

    GLint linked = GL_FALSE;
    pglGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLchar log[2048];
        GLsizei logLength = 0;
        pglGetProgramInfoLog(program, (GLsizei)sizeof(log), &logLength, log);
        fprintf(stderr, "Floor shader link failed: %.*s\n", (int)logLength, log);
        pglDeleteProgram(program);
        return 0;
    }

    return program;
}

bool glRendererInitialize(void) {
    bool loaded =
        LOAD_GL_FUNCTION(pglCreateShader, "glCreateShader")
        && LOAD_GL_FUNCTION(pglShaderSource, "glShaderSource")
        && LOAD_GL_FUNCTION(pglCompileShader, "glCompileShader")
        && LOAD_GL_FUNCTION(pglGetShaderiv, "glGetShaderiv")
        && LOAD_GL_FUNCTION(pglGetShaderInfoLog, "glGetShaderInfoLog")
        && LOAD_GL_FUNCTION(pglDeleteShader, "glDeleteShader")
        && LOAD_GL_FUNCTION(pglCreateProgram, "glCreateProgram")
        && LOAD_GL_FUNCTION(pglAttachShader, "glAttachShader")
        && LOAD_GL_FUNCTION(pglLinkProgram, "glLinkProgram")
        && LOAD_GL_FUNCTION(pglGetProgramiv, "glGetProgramiv")
        && LOAD_GL_FUNCTION(pglGetProgramInfoLog, "glGetProgramInfoLog")
        && LOAD_GL_FUNCTION(pglDeleteProgram, "glDeleteProgram")
        && LOAD_GL_FUNCTION(pglUseProgram, "glUseProgram")
        && LOAD_GL_FUNCTION(pglGetUniformLocation, "glGetUniformLocation")
        && LOAD_GL_FUNCTION(pglUniform1i, "glUniform1i")
        && LOAD_GL_FUNCTION(pglUniform1f, "glUniform1f")
        && LOAD_GL_FUNCTION(pglUniform2f, "glUniform2f")
        && LOAD_GL_FUNCTION_ALIAS(pglGenFramebuffers,
            "glGenFramebuffers", "glGenFramebuffersEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglDeleteFramebuffers,
            "glDeleteFramebuffers", "glDeleteFramebuffersEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglBindFramebuffer,
            "glBindFramebuffer", "glBindFramebufferEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglFramebufferTexture2D,
            "glFramebufferTexture2D", "glFramebufferTexture2DEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglCheckFramebufferStatus,
            "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglGenRenderbuffers,
            "glGenRenderbuffers", "glGenRenderbuffersEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglDeleteRenderbuffers,
            "glDeleteRenderbuffers", "glDeleteRenderbuffersEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglBindRenderbuffer,
            "glBindRenderbuffer", "glBindRenderbufferEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglRenderbufferStorage,
            "glRenderbufferStorage", "glRenderbufferStorageEXT")
        && LOAD_GL_FUNCTION_ALIAS(pglFramebufferRenderbuffer,
            "glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT");
    if (!loaded) {
        return false;
    }

    floorProgram = compileFloorProgram();
    if (floorProgram == 0) {
        return false;
    }
    floorTextureUniform = pglGetUniformLocation(floorProgram, "floorTexture");
    floorPositionUniform = pglGetUniformLocation(floorProgram, "playerPosition");
    floorDirectionUniform = pglGetUniformLocation(floorProgram, "direction");
    floorPlaneUniform = pglGetUniformLocation(floorProgram, "cameraPlane");
    floorCameraOffsetUniform = pglGetUniformLocation(floorProgram, "cameraOffsetY");
    floorResolutionUniform = pglGetUniformLocation(floorProgram, "resolution");
    if (floorTextureUniform < 0 || floorPositionUniform < 0 || floorDirectionUniform < 0
        || floorPlaneUniform < 0 || floorCameraOffsetUniform < 0
        || floorResolutionUniform < 0) {
        fprintf(stderr, "Failed to get floor shader uniforms.\n");
        glRendererShutdown();
        return false;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    return true;
}

void glRendererShutdown(void) {
    if (floorProgram != 0 && pglDeleteProgram != NULL) {
        pglDeleteProgram(floorProgram);
        floorProgram = 0;
    }
}

GLuint glLoadTexture(const char *path, int *width, int *height) {
    SDL_Surface *loadedSurface = IMG_Load(path);
    if (loadedSurface == NULL) {
        fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
        return 0;
    }
    SDL_Surface *surface = SDL_ConvertSurfaceFormat(
        loadedSurface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loadedSurface);
    if (surface == NULL) {
        fprintf(stderr, "SDL_ConvertSurfaceFormat Error: %s\n", SDL_GetError());
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    if (texture == 0) {
        fprintf(stderr, "glGenTextures Error while loading %s\n", path);
        SDL_FreeSurface(surface);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        if (width != NULL) {
            *width = surface->w;
        }
        if (height != NULL) {
            *height = surface->h;
        }
    }
    SDL_FreeSurface(surface);
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL texture upload failed for %s: 0x%x\n", path, error);
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

GLuint glLoadTextureFromMemory(const void *imageData, size_t imageSize) {
    if (imageData == NULL || imageSize == 0 || imageSize > INT_MAX) {
        fprintf(stderr, "Invalid in-memory texture data.\n");
        return 0;
    }

    SDL_RWops *imageStream = SDL_RWFromConstMem(imageData, (int)imageSize);
    if (imageStream == NULL) {
        fprintf(stderr, "SDL_RWFromConstMem Error: %s\n", SDL_GetError());
        return 0;
    }
    SDL_Surface *loadedSurface = IMG_Load_RW(imageStream, 1);
    if (loadedSurface == NULL) {
        fprintf(stderr, "IMG_Load_RW Error: %s\n", IMG_GetError());
        return 0;
    }
    SDL_Surface *surface = SDL_ConvertSurfaceFormat(
        loadedSurface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loadedSurface);
    if (surface == NULL) {
        fprintf(stderr, "SDL_ConvertSurfaceFormat Error: %s\n", SDL_GetError());
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        fprintf(stderr, "glGenTextures Error while loading embedded texture\n");
        SDL_FreeSurface(surface);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h,
        0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    GLenum error = glGetError();
    SDL_FreeSurface(surface);
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL embedded texture upload failed: 0x%x\n", error);
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

void glDeleteTexture(GLuint texture) {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }
}

bool glCreateRenderTarget(GLRenderTarget *target, int width, int height) {
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid OpenGL render target size: %d x %d\n", width, height);
        return false;
    }
    glDestroyRenderTarget(target);

    glGenTextures(1, &target->texture);
    if (target->texture == 0) {
        fprintf(stderr, "glGenTextures render target Error\n");
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, target->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    pglGenRenderbuffers(1, &target->depthBuffer);
    if (target->depthBuffer == 0) {
        fprintf(stderr, "glGenRenderbuffers Error\n");
        glDestroyRenderTarget(target);
        return false;
    }
    pglBindRenderbuffer(GL_RENDERBUFFER, target->depthBuffer);
    pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    pglBindRenderbuffer(GL_RENDERBUFFER, 0);

    pglGenFramebuffers(1, &target->framebuffer);
    if (target->framebuffer == 0) {
        fprintf(stderr, "glGenFramebuffers Error\n");
        glDestroyRenderTarget(target);
        return false;
    }
    pglBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, target->texture, 0);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER, target->depthBuffer);
    GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "OpenGL framebuffer is incomplete: 0x%x\n", status);
        glDestroyRenderTarget(target);
        return false;
    }

    target->width = width;
    target->height = height;
    return true;
}

void glDestroyRenderTarget(GLRenderTarget *target) {
    if (target->framebuffer != 0 && pglDeleteFramebuffers != NULL) {
        pglDeleteFramebuffers(1, &target->framebuffer);
    }
    if (target->texture != 0) {
        glDeleteTextures(1, &target->texture);
    }
    if (target->depthBuffer != 0 && pglDeleteRenderbuffers != NULL) {
        pglDeleteRenderbuffers(1, &target->depthBuffer);
    }
    memset(target, 0, sizeof(*target));
}

void glSet2DProjection(int width, int height) {
    glSetLogicalProjection(width, height, width, height);
}

GLfloat glDepthZFromDistance(double distance) {
    const double nearPlane = 0.05;
    const double farPlane = 100.0;
    if (distance < nearPlane) {
        distance = nearPlane;
    } else if (distance > farPlane) {
        distance = farPlane;
    }
    double depth = farPlane / (farPlane - nearPlane)
        - (farPlane * nearPlane) / ((farPlane - nearPlane) * distance);
    return (GLfloat)(1.0 - 2.0 * depth);
}

void glSetLogicalProjection(int viewportWidth, int viewportHeight,
    int logicalWidth, int logicalHeight) {
    if (viewportWidth <= 0 || viewportHeight <= 0
        || logicalWidth <= 0 || logicalHeight <= 0) {
        return;
    }

    float scaleX = (float)viewportWidth / logicalWidth;
    float scaleY = (float)viewportHeight / logicalHeight;
    float scale = fminf(scaleX, scaleY);
    int scaledWidth = (int)(logicalWidth * scale);
    int scaledHeight = (int)(logicalHeight * scale);
    glViewport((viewportWidth - scaledWidth) / 2,
        (viewportHeight - scaledHeight) / 2, scaledWidth, scaledHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, logicalWidth, logicalHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void glBeginRenderTarget(const GLRenderTarget *target) {
    pglBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
    glSet2DProjection(target->width, target->height);
    glClearColor(42.0f / 255.0f, 48.0f / 255.0f, 68.0f / 255.0f, 1.0f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void glDrawFloor(GLuint texture, double playerX, double playerY,
    double directionX, double directionY, double planeX, double planeY,
    double cameraOffsetY, int width, int height) {
    pglUseProgram(floorProgram);
    glBindTexture(GL_TEXTURE_2D, texture);
    pglUniform1i(floorTextureUniform, 0);
    pglUniform2f(floorPositionUniform, (GLfloat)playerX, (GLfloat)playerY);
    pglUniform2f(floorDirectionUniform, (GLfloat)directionX, (GLfloat)directionY);
    pglUniform2f(floorPlaneUniform, (GLfloat)planeX, (GLfloat)planeY);
    pglUniform1f(floorCameraOffsetUniform, (GLfloat)cameraOffsetY);
    pglUniform2f(floorResolutionUniform, (GLfloat)width, (GLfloat)height);
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f((GLfloat)width, 0.0f);
    glVertex2f((GLfloat)width, (GLfloat)height);
    glVertex2f(0.0f, (GLfloat)height);
    glEnd();
    pglUseProgram(0);
    glEnable(GL_BLEND);
}

void glPresentRenderTarget(const GLRenderTarget *target, int outputWidth, int outputHeight) {
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
    glSet2DProjection(outputWidth, outputHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, target->texture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f((GLfloat)outputWidth, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f((GLfloat)outputWidth, (GLfloat)outputHeight);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, (GLfloat)outputHeight);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void glRendererClear(Uint8 red, Uint8 green, Uint8 blue) {
    glClearColor(red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void glFillRect(float x, float y, float width, float height, SDL_Color color) {
    glDisable(GL_TEXTURE_2D);
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void glOutlineRect(float x, float y, float width, float height, SDL_Color color) {
    glDisable(GL_TEXTURE_2D);
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void glDrawLine(float x1, float y1, float x2, float y2, SDL_Color color) {
    glDisable(GL_TEXTURE_2D);
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void glDrawTexture(GLuint texture, float x, float y, float width, float height,
    float angleDegrees, float pivotX, float pivotY, Uint8 alpha) {
    if (texture == 0 || width <= 0.0f || height <= 0.0f) {
        return;
    }
    float radians = angleDegrees * 0.01745329251994329577f;
    float cosine = cosf(radians);
    float sine = sinf(radians);
    float centerX = x + pivotX;
    float centerY = y + pivotY;
    const float localX[4] = {0.0f, width, width, 0.0f};
    const float localY[4] = {0.0f, 0.0f, height, height};
    const float textureX[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    const float textureY[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4ub(255, 255, 255, alpha);
    glBegin(GL_QUADS);
    for (int vertex = 0; vertex < 4; vertex++) {
        float offsetX = localX[vertex] - pivotX;
        float offsetY = localY[vertex] - pivotY;
        float rotatedX = centerX + offsetX * cosine - offsetY * sine;
        float rotatedY = centerY + offsetX * sine + offsetY * cosine;
        glTexCoord2f(textureX[vertex], textureY[vertex]);
        glVertex2f(rotatedX, rotatedY);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void glSwapWindow(SDL_Window *window) {
    SDL_GL_SwapWindow(window);
}

int glSetSwapInterval(bool enabled) {
    return SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}
