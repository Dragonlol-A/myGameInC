#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include "gl_renderer.h"
#include "player.h"

typedef struct {
    float x;
    float y;
    float z;
    float normalX;
    float normalY;
    float normalZ;
    float u;
    float v;
} ModelVertex;

typedef struct {
    GLuint texture;
    float baseColor[4];
    bool alphaMask;
    bool alphaBlend;
    float alphaCutoff;
} ModelMaterial;

typedef struct {
    size_t firstVertex;
    size_t vertexCount;
    size_t materialIndex;
    size_t drawIndexCount;
    size_t firstIndex;
    size_t indexCount;
} ModelBatch;

typedef struct {
    float x;
    float y;
    float z;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
    float light;
    bool visible;
} ModelRenderVertex;

typedef struct {
    ModelVertex *vertices;
    size_t vertexCount;
    GLuint *indices;
    size_t indexCount;
    GLuint *renderIndices;
    ModelRenderVertex *renderVertices;
    ModelMaterial *materials;
    size_t materialCount;
    ModelBatch *batches;
    size_t batchCount;
    float centerX;
    float centerZ;
    float minY;
} Model;

typedef Model ObjModel;

typedef struct {
    const Model *model;
    double worldX;
    double worldY;
    float scale;
} ModelInstance;

bool modelLoad(Model *model, const char *path);
bool gltfModelLoad(Model *model, const char *path);
bool objModelLoad(ObjModel *model, const char *path);
bool modelPrepareRenderCache(Model *model);
void modelDestroy(Model *model);
void modelDraw(const ModelInstance *instance, const Player *player,
    int renderWidth, int renderHeight, double cameraOffsetY);

#endif
