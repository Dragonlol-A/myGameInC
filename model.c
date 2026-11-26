#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl_renderer.h"
#include "model.h"

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    size_t position;
    size_t normal;
    bool hasNormal;
} FaceVertex;

static bool reserveArray(void **items, size_t *capacity, size_t required,
    size_t itemSize) {
    if (required <= *capacity) {
        return true;
    }

    size_t newCapacity = *capacity == 0 ? 16 : *capacity;
    while (newCapacity < required) {
        if (newCapacity > (size_t)-1 / 2) {
            return false;
        }
        newCapacity *= 2;
    }
    if (newCapacity > (size_t)-1 / itemSize) {
        return false;
    }

    void *newItems = realloc(*items, newCapacity * itemSize);
    if (newItems == NULL) {
        return false;
    }
    *items = newItems;
    *capacity = newCapacity;
    return true;
}

static bool appendVec3(Vec3 **items, size_t *count, size_t *capacity, Vec3 value) {
    if (!reserveArray((void **)items, capacity, *count + 1, sizeof(**items))) {
        return false;
    }
    (*items)[(*count)++] = value;
    return true;
}

static bool appendVertex(ObjModel *model, size_t *capacity, ModelVertex vertex) {
    if (!reserveArray((void **)&model->vertices, capacity,
            model->vertexCount + 1, sizeof(*model->vertices))) {
        return false;
    }
    model->vertices[model->vertexCount++] = vertex;
    return true;
}

static bool appendIndex(Model *model, size_t *capacity, size_t index) {
    if (index > UINT_MAX || model->indexCount == (size_t)-1 || !reserveArray((void **)&model->indices, capacity, model->indexCount + 1, sizeof(*model->indices))) {
        return false;
    }
    model->indices[model->indexCount++] = (GLuint)index;
    return true;
}

static bool resolveIndex(const char *text, size_t count, size_t *index) {
    errno = 0;
    char *end = NULL;
    long rawIndex = strtol(text, &end, 10);
    if (text == end || *end != '\0' || errno == ERANGE || rawIndex == 0 || rawIndex > INT_MAX || rawIndex < INT_MIN) {
        return false;
    }

    long resolved = rawIndex > 0 ? rawIndex - 1 : (long)count + rawIndex;
    if (resolved < 0 || (size_t)resolved >= count) {
        return false;
    }
    *index = (size_t)resolved;
    return true;
}

static bool parseFaceVertex(char *text, size_t positionCount, size_t normalCount,
    FaceVertex *vertex) {
    char *firstSlash = strchr(text, '/');
    char *secondSlash = firstSlash == NULL ? NULL : strchr(firstSlash + 1, '/');
    if (firstSlash != NULL) {
        *firstSlash = '\0';
    }
    if (!resolveIndex(text, positionCount, &vertex->position)) {
        return false;
    }

    vertex->hasNormal = false;
    if (secondSlash != NULL && secondSlash[1] != '\0') {
        if (!resolveIndex(secondSlash + 1, normalCount, &vertex->normal)) {
            return false;
        }
        vertex->hasNormal = true;
    }
    return true;
}

static Vec3 subtract(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vec3 cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static Vec3 normalized(Vec3 value) {
    float length = sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.000001f) {
        return (Vec3){0.0f, 1.0f, 0.0f};
    }
    return (Vec3){value.x / length, value.y / length, value.z / length};
}

static bool appendFaceTriangle(ObjModel *model, size_t *vertexCapacity,
    size_t *indexCapacity, const Vec3 *positions, const Vec3 *normals,
    const FaceVertex *a, const FaceVertex *b, const FaceVertex *c) {
    Vec3 p0 = positions[a->position];
    Vec3 p1 = positions[b->position];
    Vec3 p2 = positions[c->position];
    Vec3 faceNormal = normalized(cross(subtract(p1, p0), subtract(p2, p0)));
    const FaceVertex *corners[] = {a, b, c};

    for (size_t i = 0; i < 3; i++) {
        Vec3 position = positions[corners[i]->position];
        Vec3 normal = corners[i]->hasNormal ? normalized(normals[corners[i]->normal]) : faceNormal;
        size_t index = model->vertexCount;
        ModelVertex vertex = {
            position.x, position.y, position.z,
            normal.x, normal.y, normal.z, 0.0f, 0.0f
        };
        if (!appendVertex(model, vertexCapacity, vertex) || !appendIndex(model, indexCapacity, index)) {
            return false;
        }
    }
    return true;
}

static bool appendFace(ObjModel *model, size_t *vertexCapacity,
    size_t *indexCapacity, const Vec3 *positions, size_t positionCount,
    const Vec3 *normals, size_t normalCount, char *line) {
    FaceVertex *face = NULL;
    size_t faceCount = 0;
    size_t faceCapacity = 0;
    bool valid = true;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL) {
        if (token[0] == '#') {
            break;
        }
        FaceVertex vertex;
        if (!parseFaceVertex(token, positionCount, normalCount, &vertex) || !reserveArray((void **)&face, &faceCapacity, faceCount + 1, sizeof(*face))) {
            valid = false;
            break;
        }
        face[faceCount++] = vertex;
        token = strtok(NULL, " \t\r\n");
    }

    if (valid && faceCount < 3) {
        valid = false;
    }
    for (size_t i = 1; valid && i + 1 < faceCount; i++) {
        if (!appendFaceTriangle(model, vertexCapacity, indexCapacity,
                positions, normals, &face[0], &face[i], &face[i + 1])) {
            valid = false;
        }
    }
    free(face);
    return valid;
}

void modelDestroy(Model *model) {
    if (model == NULL) {
        return;
    }
    for (size_t i = 0; i < model->materialCount; i++) {
        glDeleteTexture(model->materials[i].texture);
    }
    free(model->vertices);
    free(model->indices);
    free(model->renderIndices);
    free(model->renderVertices);
    free(model->materials);
    free(model->batches);
    memset(model, 0, sizeof(*model));
}

bool modelPrepareRenderCache(Model *model) {
    if (model == NULL || model->vertexCount == 0 || model->indexCount == 0
        || model->materials == NULL || model->materialCount == 0
        || model->batches == NULL || model->batchCount == 0
        || model->vertexCount > (size_t)-1 / sizeof(*model->renderVertices)
        || model->indexCount > (size_t)-1 / sizeof(*model->renderIndices)) {
        fprintf(stderr, "Invalid model data for render cache.\n");
        return false;
    }
    if (model->indexCount > INT_MAX) {
        fprintf(stderr, "Model has too many indices for OpenGL drawing.\n");
        return false;
    }
    for (size_t i = 0; i < model->batchCount; i++) {
        const ModelBatch *batch = &model->batches[i];
        if (batch->indexCount % 3 != 0
            || batch->firstIndex > model->indexCount
            || batch->indexCount > model->indexCount - batch->firstIndex
            || batch->indexCount > INT_MAX) {
            fprintf(stderr, "Model contains an invalid triangle batch.\n");
            return false;
        }
    }

    model->renderVertices = calloc(model->vertexCount,
        sizeof(*model->renderVertices));
    model->renderIndices = malloc(model->indexCount
        * sizeof(*model->renderIndices));
    if (model->renderVertices == NULL || model->renderIndices == NULL) {
        fprintf(stderr, "Failed to allocate model render cache.\n");
        free(model->renderVertices);
        free(model->renderIndices);
        model->renderVertices = NULL;
        model->renderIndices = NULL;
        return false;
    }
    return true;
}

bool objModelLoad(ObjModel *model, const char *path) {
    if (model == NULL || path == NULL) {
        fprintf(stderr, "Invalid OBJ model load arguments.\n");
        return false;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open OBJ model %s.\n", path);
        return false;
    }

    Vec3 *positions = NULL;
    Vec3 *normals = NULL;
    size_t positionCount = 0;
    size_t normalCount = 0;
    size_t positionCapacity = 0;
    size_t normalCapacity = 0;
    size_t vertexCapacity = 0;
    size_t indexCapacity = 0;
    size_t lineNumber = 0;
    char line[4096];
    bool valid = true;
    ObjModel loaded = {0};

    while (valid && fgets(line, sizeof(line), file) != NULL) {
        lineNumber++;
        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "OBJ line too long in %s at line %zu.\n",
                path, lineNumber);
            valid = false;
            break;
        }

        char *cursor = line;
        while (isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor == '\0' || *cursor == '#') {
            continue;
        }

        if (cursor[0] == 'v' && cursor[1] == 'n'
            && isspace((unsigned char)cursor[2])) {
            Vec3 normal;
            if (sscanf(cursor + 2, "%f %f %f",
                    &normal.x, &normal.y, &normal.z) != 3
                || !appendVec3(&normals, &normalCount,
                    &normalCapacity, normal)) {
                valid = false;
            }
        } else if (cursor[0] == 'v' && isspace((unsigned char)cursor[1])) {
            Vec3 position;
            if (sscanf(cursor + 1, "%f %f %f",
                    &position.x, &position.y, &position.z) != 3
                || !appendVec3(&positions, &positionCount,
                    &positionCapacity, position)) {
                valid = false;
            }
        } else if (cursor[0] == 'f' && isspace((unsigned char)cursor[1])) {
            if (!appendFace(&loaded, &vertexCapacity, &indexCapacity,
                    positions, positionCount, normals, normalCount, cursor + 1)) {
                valid = false;
            }
        }

        if (!valid) {
            fprintf(stderr, "Invalid or out-of-memory OBJ data in %s at line %zu.\n",
                path, lineNumber);
        }
    }

    if (ferror(file)) {
        fprintf(stderr, "Error reading OBJ model %s.\n", path);
        valid = false;
    }
    fclose(file);
    free(positions);
    free(normals);

    if (valid && loaded.vertexCount == 0) {
        fprintf(stderr, "OBJ model contains no faces: %s.\n", path);
        valid = false;
    }

    if (valid) {
        float minX = loaded.vertices[0].x;
        float maxX = minX;
        float minY = loaded.vertices[0].y;
        float minZ = loaded.vertices[0].z;
        float maxZ = minZ;
        for (size_t i = 1; i < loaded.vertexCount; i++) {
            const ModelVertex *vertex = &loaded.vertices[i];
            if (vertex->x < minX) minX = vertex->x;
            if (vertex->x > maxX) maxX = vertex->x;
            if (vertex->y < minY) minY = vertex->y;
            if (vertex->z < minZ) minZ = vertex->z;
            if (vertex->z > maxZ) maxZ = vertex->z;
        }
        loaded.centerX = (minX + maxX) * 0.5f;
        loaded.centerZ = (minZ + maxZ) * 0.5f;
        loaded.minY = minY;
        loaded.materials = calloc(1, sizeof(*loaded.materials));
        loaded.batches = calloc(1, sizeof(*loaded.batches));
        if (loaded.materials == NULL || loaded.batches == NULL) {
            fprintf(stderr, "Failed to allocate OBJ model material data.\n");
            modelDestroy(&loaded);
            return false;
        }
        loaded.materialCount = 1;
        loaded.materials[0].baseColor[0] = 0.8f;
        loaded.materials[0].baseColor[1] = 0.84f;
        loaded.materials[0].baseColor[2] = 1.0f;
        loaded.materials[0].baseColor[3] = 1.0f;
        loaded.batches[0] = (ModelBatch){
            0, loaded.vertexCount, 0, 0, 0, loaded.indexCount
        };
        loaded.batchCount = 1;
        if (!modelPrepareRenderCache(&loaded)) {
            modelDestroy(&loaded);
            return false;
        }
        modelDestroy(model);
        *model = loaded;
        return true;
    }

    modelDestroy(&loaded);
    return false;
}

bool modelLoad(Model *model, const char *path) {
    if (model == NULL || path == NULL) {
        fprintf(stderr, "Invalid model load arguments.\n");
        return false;
    }

    const char *extension = strrchr(path, '.');
    if (extension != NULL && (strcmp(extension, ".glb") == 0
            || strcmp(extension, ".gltf") == 0)) {
        return gltfModelLoad(model, path);
    }
    return objModelLoad(model, path);
}

static bool projectVertex(const Model *model, const ModelVertex *vertex,
    const Player *player, int renderWidth, int renderHeight,
    double cameraOffsetY, double inverseDeterminant,
    double worldX, double worldY, float scale,
    float *screenX, float *screenY, float *depthZ) {
    double pointX = worldX + (vertex->x - model->centerX) * scale;
    double pointY = worldY - (vertex->z - model->centerZ) * scale;
    double pointZ = (vertex->y - model->minY) * scale;
    double relativeX = pointX - player->x;
    double relativeY = pointY - player->y;
    double transformX = inverseDeterminant
        * (player->dirY * relativeX - player->dirX * relativeY);
    double transformY = inverseDeterminant
        * (-player->planeY * relativeX + player->planeX * relativeY);
    if (transformY <= 0.05 || transformY >= 100.0) {
        return false;
    }

    *screenX = (float)((renderWidth / 2.0) * (1.0 + transformX / transformY));
    *screenY = (float)(renderHeight / 2.0
        - (pointZ - 0.5) * renderHeight / transformY + cameraOffsetY);
    *depthZ = glDepthZFromDistance(transformY);
    return true;
}

void modelDraw(const ModelInstance *instance, const Player *player,
    int renderWidth, int renderHeight, double cameraOffsetY) {
    if (instance == NULL || instance->model == NULL
        || instance->model->vertices == NULL
        || instance->model->materials == NULL
        || instance->model->materialCount == 0
        || instance->model->renderVertices == NULL
        || instance->model->indices == NULL
        || instance->model->renderIndices == NULL || player == NULL
        || renderWidth <= 0 || renderHeight <= 0 || instance->scale <= 0.0f) {
        return;
    }

    const Model *model = instance->model;
    double determinant = player->planeX * player->dirY
        - player->dirX * player->planeY;
    if (fabs(determinant) < 0.000001) {
        return;
    }
    double inverseDeterminant = 1.0 / determinant;
    const float lightX = 0.35f;
    const float lightY = -0.45f;
    const float lightZ = 0.82f;
    for (size_t i = 0; i < model->vertexCount; i++) {
        const ModelVertex *vertex = &model->vertices[i];
        ModelRenderVertex *renderVertex = &model->renderVertices[i];
        renderVertex->visible = projectVertex(model, vertex, player,
            renderWidth, renderHeight, cameraOffsetY, inverseDeterminant,
            instance->worldX, instance->worldY, instance->scale, &renderVertex->x,
            &renderVertex->y, &renderVertex->z);
        renderVertex->u = vertex->u;
        renderVertex->v = vertex->v;
        float normalX = vertex->normalX;
        float normalY = -vertex->normalZ;
        float normalZ = vertex->normalY;
        float diffuse = fmaxf(0.0f,
            normalX * lightX + normalY * lightY + normalZ * lightZ);
        renderVertex->light = 0.3f + diffuse * 0.7f;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(*model->renderVertices),
        &model->renderVertices[0].x);
    glColorPointer(4, GL_FLOAT, sizeof(*model->renderVertices),
        &model->renderVertices[0].r);
    glTexCoordPointer(2, GL_FLOAT, sizeof(*model->renderVertices),
        &model->renderVertices[0].u);

    for (size_t batchIndex = 0; batchIndex < model->batchCount; batchIndex++) {
        ModelBatch *batch = &model->batches[batchIndex];
        if (batch->materialIndex >= model->materialCount
            || batch->firstIndex > model->indexCount
            || batch->indexCount > model->indexCount - batch->firstIndex) {
            continue;
        }
        const ModelMaterial *material = &model->materials[batch->materialIndex];
        batch->drawIndexCount = 0;
        for (size_t i = 0; i < batch->indexCount; i += 3) {
            GLuint index0 = model->indices[batch->firstIndex + i];
            GLuint index1 = model->indices[batch->firstIndex + i + 1];
            GLuint index2 = model->indices[batch->firstIndex + i + 2];
            if (index0 >= model->vertexCount || index1 >= model->vertexCount
                || index2 >= model->vertexCount
                || !model->renderVertices[index0].visible
                || !model->renderVertices[index1].visible
                || !model->renderVertices[index2].visible) {
                continue;
            }
            const GLuint triangle[] = {index0, index1, index2};
            for (size_t corner = 0; corner < 3; corner++) {
                ModelRenderVertex *renderVertex =
                    &model->renderVertices[triangle[corner]];
                renderVertex->r = material->baseColor[0] * renderVertex->light;
                renderVertex->g = material->baseColor[1] * renderVertex->light;
                renderVertex->b = material->baseColor[2] * renderVertex->light;
                renderVertex->a = material->baseColor[3];
            }
            size_t outputIndex = batch->firstIndex + batch->drawIndexCount;
            model->renderIndices[outputIndex++] = index0;
            model->renderIndices[outputIndex++] = index1;
            model->renderIndices[outputIndex++] = index2;
            batch->drawIndexCount += 3;
        }
        if (batch->drawIndexCount == 0) {
            continue;
        }

        if (material->texture != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, material->texture);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
        if (material->alphaMask) {
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, material->alphaCutoff);
        } else {
            glDisable(GL_ALPHA_TEST);
        }
        glDepthMask(material->alphaBlend ? GL_FALSE : GL_TRUE);
        glDrawElements(GL_TRIANGLES, (GLsizei)batch->drawIndexCount,
            GL_UNSIGNED_INT, model->renderIndices + batch->firstIndex);
        glDepthMask(GL_TRUE);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
}
