#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl_renderer.h"
#include "model.h"

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

static bool appendVertex(Model *model, size_t *capacity, ModelVertex vertex) {
    if (!reserveArray((void **)&model->vertices, capacity,
            model->vertexCount + 1, sizeof(*model->vertices))) {
        return false;
    }
    model->vertices[model->vertexCount++] = vertex;
    return true;
}

static bool appendIndex(Model *model, size_t *capacity, size_t index) {
    if (index > UINT_MAX || model->indexCount == (size_t)-1
        || !reserveArray((void **)&model->indices, capacity,
            model->indexCount + 1, sizeof(*model->indices))) {
        return false;
    }
    model->indices[model->indexCount++] = (GLuint)index;
    return true;
}

static bool appendBatch(Model *model, size_t *capacity, ModelBatch batch) {
    if (!reserveArray((void **)&model->batches, capacity,
            model->batchCount + 1, sizeof(*model->batches))) {
        return false;
    }
    model->batches[model->batchCount++] = batch;
    return true;
}

static void transformPoint(const cgltf_float matrix[16], const float input[3],
    float output[3]) {
    output[0] = matrix[0] * input[0] + matrix[4] * input[1]
        + matrix[8] * input[2] + matrix[12];
    output[1] = matrix[1] * input[0] + matrix[5] * input[1]
        + matrix[9] * input[2] + matrix[13];
    output[2] = matrix[2] * input[0] + matrix[6] * input[1]
        + matrix[10] * input[2] + matrix[14];
}

static void transformNormal(const cgltf_float matrix[16], const float input[3],
    float output[3]) {
    float a00 = matrix[0], a01 = matrix[4], a02 = matrix[8];
    float a10 = matrix[1], a11 = matrix[5], a12 = matrix[9];
    float a20 = matrix[2], a21 = matrix[6], a22 = matrix[10];
    float c00 = a11 * a22 - a12 * a21;
    float c01 = a12 * a20 - a10 * a22;
    float c02 = a10 * a21 - a11 * a20;
    float c10 = a02 * a21 - a01 * a22;
    float c11 = a00 * a22 - a02 * a20;
    float c12 = a01 * a20 - a00 * a21;
    float c20 = a01 * a12 - a02 * a11;
    float c21 = a02 * a10 - a00 * a12;
    float c22 = a00 * a11 - a01 * a10;
    float determinant = a00 * c00 + a01 * c01 + a02 * c02;
    if (fabsf(determinant) < 0.000001f) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];
    } else {
        output[0] = (c00 * input[0] + c01 * input[1] + c02 * input[2])
            / determinant;
        output[1] = (c10 * input[0] + c11 * input[1] + c12 * input[2])
            / determinant;
        output[2] = (c20 * input[0] + c21 * input[1] + c22 * input[2])
            / determinant;
    }
    float length = sqrtf(output[0] * output[0] + output[1] * output[1]
        + output[2] * output[2]);
    if (length > 0.000001f) {
        output[0] /= length;
        output[1] /= length;
        output[2] /= length;
    }
}

static const cgltf_accessor *findAttribute(const cgltf_primitive *primitive,
    cgltf_attribute_type type, cgltf_int index) {
    for (cgltf_size i = 0; i < primitive->attributes_count; i++) {
        const cgltf_attribute *attribute = &primitive->attributes[i];
        if (attribute->type == type && attribute->index == index) {
            return attribute->data;
        }
    }
    return NULL;
}

static char *resolveImagePath(const char *modelPath, const char *uri) {
    size_t uriLength = strlen(uri);
    char *decodedUri = malloc(uriLength + 1);
    if (decodedUri == NULL) {
        fprintf(stderr, "Failed to allocate texture path for %s.\n", modelPath);
        return NULL;
    }
    memcpy(decodedUri, uri, uriLength + 1);
    cgltf_size decodedLength = cgltf_decode_uri(decodedUri);
    decodedUri[decodedLength] = '\0';

    if (decodedUri[0] == '/' || decodedUri[0] == '\\'
        || (decodedLength > 1 && decodedUri[1] == ':')) {
        return decodedUri;
    }

    const char *forwardSlash = strrchr(modelPath, '/');
    const char *backSlash = strrchr(modelPath, '\\');
    const char *lastSlash = forwardSlash;
    if (backSlash != NULL && (lastSlash == NULL || backSlash > lastSlash)) {
        lastSlash = backSlash;
    }
    size_t directoryLength = lastSlash == NULL
        ? 0 : (size_t)(lastSlash - modelPath + 1);
    if (directoryLength > (size_t)-1 - decodedLength - 1) {
        free(decodedUri);
        fprintf(stderr, "Texture path is too long in %s.\n", modelPath);
        return NULL;
    }
    char *resolvedPath = malloc(directoryLength + decodedLength + 1);
    if (resolvedPath == NULL) {
        free(decodedUri);
        fprintf(stderr, "Failed to allocate texture path for %s.\n", modelPath);
        return NULL;
    }
    if (directoryLength != 0) {
        memcpy(resolvedPath, modelPath, directoryLength);
    }
    memcpy(resolvedPath + directoryLength, decodedUri, decodedLength + 1);
    free(decodedUri);
    return resolvedPath;
}

static bool readTextureFile(const char *path, void **contents, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open glTF texture %s.\n", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Could not seek in glTF texture %s.\n", path);
        fclose(file);
        return false;
    }
    long fileSize = ftell(file);
    if (fileSize <= 0 || (unsigned long)fileSize > INT_MAX
        || fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Invalid glTF texture size: %s.\n", path);
        fclose(file);
        return false;
    }

    void *data = malloc((size_t)fileSize);
    if (data == NULL) {
        fprintf(stderr, "Failed to allocate glTF texture data for %s.\n", path);
        fclose(file);
        return false;
    }
    if (fread(data, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        fprintf(stderr, "Could not read glTF texture %s.\n", path);
        free(data);
        fclose(file);
        return false;
    }
    fclose(file);
    *contents = data;
    *size = (size_t)fileSize;
    return true;
}

static GLuint loadExternalTexture(const cgltf_image *image,
    const char *modelPath) {
    if (image->uri == NULL || strncmp(image->uri, "data:", 5) == 0) {
        fprintf(stderr, "Unsupported glTF texture URI in %s.\n", modelPath);
        return 0;
    }

    char *texturePath = resolveImagePath(modelPath, image->uri);
    if (texturePath == NULL) {
        return 0;
    }
    void *imageData = NULL;
    size_t imageSize = 0;
    if (!readTextureFile(texturePath, &imageData, &imageSize)) {
        free(texturePath);
        return 0;
    }
    GLuint texture = glLoadTextureFromMemory(imageData, imageSize);
    free(imageData);
    free(texturePath);
    return texture;
}

static GLuint loadGltfTexture(const cgltf_image *image, const char *path) {
    if (image == NULL) {
        fprintf(stderr, "glTF material in %s references a missing image.\n", path);
        return 0;
    }
    if (image->buffer_view != NULL) {
        const uint8_t *imageData = cgltf_buffer_view_data(image->buffer_view);
        if (imageData == NULL) {
            fprintf(stderr, "Embedded texture data is missing in %s.\n", path);
            return 0;
        }
        return glLoadTextureFromMemory(imageData, image->buffer_view->size);
    }
    return loadExternalTexture(image, path);
}

static bool loadMaterials(Model *model, const cgltf_data *data,
    const char *path) {
    size_t count = data->materials_count + 1;
    if (count < data->materials_count
        || count > (size_t)-1 / sizeof(*model->materials)) {
        fprintf(stderr, "Too many materials in GLB: %s.\n", path);
        return false;
    }

    model->materials = calloc(count, sizeof(*model->materials));
    if (model->materials == NULL) {
        fprintf(stderr, "Failed to allocate GLB materials for %s.\n", path);
        return false;
    }
    model->materialCount = count;
    for (size_t i = 0; i < count; i++) {
        model->materials[i].baseColor[0] = 1.0f;
        model->materials[i].baseColor[1] = 1.0f;
        model->materials[i].baseColor[2] = 1.0f;
        model->materials[i].baseColor[3] = 1.0f;
        model->materials[i].alphaCutoff = 0.5f;
    }

    for (cgltf_size i = 0; i < data->materials_count; i++) {
        const cgltf_material *source = &data->materials[i];
        ModelMaterial *destination = &model->materials[i + 1];
        if (source->has_pbr_metallic_roughness) {
            memcpy(destination->baseColor,
                source->pbr_metallic_roughness.base_color_factor,
                sizeof(destination->baseColor));
            const cgltf_texture_view *view =
                &source->pbr_metallic_roughness.base_color_texture;
            if (view->texture != NULL) {
                if (view->texcoord != 0) {
                    fprintf(stderr,
                        "GLB material in %s uses unsupported UV set %d.\n",
                        path, view->texcoord);
                    return false;
                }
                destination->texture = loadGltfTexture(
                    view->texture->image, path);
                if (destination->texture == 0) {
                    return false;
                }
            }
        }
        destination->alphaMask = source->alpha_mode == cgltf_alpha_mode_mask;
        destination->alphaBlend = source->alpha_mode == cgltf_alpha_mode_blend;
        if (source->alpha_mode == cgltf_alpha_mode_opaque) {
            destination->baseColor[3] = 1.0f;
        }
        if (source->alpha_cutoff > 0.0f) {
            destination->alphaCutoff = source->alpha_cutoff;
        }
    }
    return true;
}

static bool readAttribute(const cgltf_accessor *accessor, cgltf_size index,
    cgltf_size expectedComponents, float *values) {
    if (accessor == NULL || accessor->is_sparse
        || cgltf_num_components(accessor->type) != expectedComponents) {
        return false;
    }
    return cgltf_accessor_read_float(accessor, index, values, expectedComponents);
}

static bool readPrimitiveVertex(const cgltf_accessor *positions,
    const cgltf_accessor *normals,
    const cgltf_accessor *texcoords, cgltf_size index,
    const cgltf_float matrix[16], ModelVertex *vertex) {
    float position[3];
    float normal[3] = {0.0f, 1.0f, 0.0f};
    float uv[2] = {0.0f, 0.0f};
    if (index >= positions->count
        || !readAttribute(positions, index, 3, position)) {
        return false;
    }
    if (normals != NULL && !readAttribute(normals, index, 3, normal)) {
        return false;
    }
    if (texcoords != NULL && !readAttribute(texcoords, index, 2, uv)) {
        return false;
    }

    float transformedPosition[3];
    float transformedNormal[3];
    transformPoint(matrix, position, transformedPosition);
    transformNormal(matrix, normal, transformedNormal);
    *vertex = (ModelVertex){
        transformedPosition[0], transformedPosition[1], transformedPosition[2],
        transformedNormal[0], transformedNormal[1], transformedNormal[2],
        uv[0], uv[1]
    };
    return true;
}

static bool appendPrimitive(Model *model, size_t *vertexCapacity,
    size_t *indexCapacity, size_t *batchCapacity, const cgltf_data *data,
    const cgltf_primitive *primitive, const cgltf_float matrix[16],
    const char *path) {
    if (primitive->type != cgltf_primitive_type_triangles) {
        fprintf(stderr, "GLB primitive in %s is not triangulated.\n", path);
        return false;
    }

    const cgltf_accessor *positions = findAttribute(primitive,
        cgltf_attribute_type_position, 0);
    const cgltf_accessor *normals = findAttribute(primitive,
        cgltf_attribute_type_normal, 0);
    cgltf_int texcoordSet = 0;
    if (primitive->material != NULL
        && primitive->material->has_pbr_metallic_roughness
        && primitive->material->pbr_metallic_roughness
            .base_color_texture.texture != NULL) {
        texcoordSet = primitive->material->pbr_metallic_roughness
            .base_color_texture.texcoord;
    }
    const cgltf_accessor *texcoords = findAttribute(primitive,
        cgltf_attribute_type_texcoord, texcoordSet);
    if (positions == NULL || positions->is_sparse
        || cgltf_num_components(positions->type) != 3) {
        fprintf(stderr, "GLB primitive in %s has no usable positions.\n", path);
        return false;
    }
    if (primitive->material != NULL
        && primitive->material->has_pbr_metallic_roughness
        && primitive->material->pbr_metallic_roughness
            .base_color_texture.texture != NULL
        && texcoords == NULL) {
        fprintf(stderr, "GLB primitive in %s is missing UV coordinates.\n", path);
        return false;
    }

    const cgltf_accessor *indices = primitive->indices;
    cgltf_size indexCount = indices != NULL ? indices->count : positions->count;
    if (indexCount == 0 || indexCount % 3 != 0
        || (indices != NULL && indices->is_sparse)) {
        fprintf(stderr, "GLB primitive in %s has invalid triangle indices.\n", path);
        return false;
    }

    size_t materialIndex = 0;
    if (primitive->material != NULL) {
        materialIndex = cgltf_material_index(data, primitive->material) + 1;
    }
    if (materialIndex >= model->materialCount) {
        fprintf(stderr, "GLB primitive in %s references an invalid material.\n", path);
        return false;
    }

    size_t firstVertex = model->vertexCount;
    size_t firstIndex = model->indexCount;
    if (normals != NULL) {
        for (cgltf_size sourceIndex = 0; sourceIndex < positions->count;
             sourceIndex++) {
            ModelVertex vertex;
            if (!readPrimitiveVertex(positions, normals, texcoords,
                    sourceIndex, matrix, &vertex)
                || !appendVertex(model, vertexCapacity, vertex)) {
                fprintf(stderr, "Failed to read GLB mesh data in %s.\n", path);
                return false;
            }
        }
        for (cgltf_size i = 0; i < indexCount; i++) {
            cgltf_size sourceIndex = indices != NULL
                ? cgltf_accessor_read_index(indices, i) : i;
            if (sourceIndex >= positions->count
                || sourceIndex > (size_t)-1 - firstVertex
                || !appendIndex(model, indexCapacity, firstVertex + sourceIndex)) {
                fprintf(stderr, "Invalid GLB index in %s.\n", path);
                return false;
            }
        }
    } else {
        for (cgltf_size i = 0; i < indexCount; i += 3) {
            ModelVertex triangle[3];
            for (cgltf_size corner = 0; corner < 3; corner++) {
                cgltf_size element = i + corner;
                cgltf_size sourceIndex = indices != NULL
                    ? cgltf_accessor_read_index(indices, element) : element;
                if (!readPrimitiveVertex(positions, NULL, texcoords,
                        sourceIndex, matrix, &triangle[corner])) {
                    fprintf(stderr, "Failed to read GLB mesh data in %s.\n", path);
                    return false;
                }
            }
            float edgeA[3] = {
                triangle[1].x - triangle[0].x,
                triangle[1].y - triangle[0].y,
                triangle[1].z - triangle[0].z
            };
            float edgeB[3] = {
                triangle[2].x - triangle[0].x,
                triangle[2].y - triangle[0].y,
                triangle[2].z - triangle[0].z
            };
            float normal[3] = {
                edgeA[1] * edgeB[2] - edgeA[2] * edgeB[1],
                edgeA[2] * edgeB[0] - edgeA[0] * edgeB[2],
                edgeA[0] * edgeB[1] - edgeA[1] * edgeB[0]
            };
            float normalLength = sqrtf(normal[0] * normal[0]
                + normal[1] * normal[1] + normal[2] * normal[2]);
            if (normalLength > 0.000001f) {
                for (size_t component = 0; component < 3; component++) {
                    normal[component] /= normalLength;
                }
            }
            for (size_t corner = 0; corner < 3; corner++) {
                triangle[corner].normalX = normal[0];
                triangle[corner].normalY = normal[1];
                triangle[corner].normalZ = normal[2];
                size_t vertexIndex = model->vertexCount;
                if (!appendVertex(model, vertexCapacity, triangle[corner])
                    || !appendIndex(model, indexCapacity, vertexIndex)) {
                    fprintf(stderr, "Failed to store GLB mesh data in %s.\n", path);
                    return false;
                }
            }
        }
    }

    ModelBatch batch = {
        firstVertex,
        model->vertexCount - firstVertex,
        materialIndex,
        0,
        firstIndex,
        model->indexCount - firstIndex
    };
    if (!appendBatch(model, batchCapacity, batch)) {
        fprintf(stderr, "Failed to store GLB mesh batch from %s.\n", path);
        return false;
    }
    return true;
}

static bool appendNode(Model *model, size_t *vertexCapacity,
    size_t *indexCapacity, size_t *batchCapacity, const cgltf_data *data,
    const cgltf_node *node, size_t depth, const char *path) {
    if (depth > 256) {
        fprintf(stderr, "GLB node hierarchy is too deep in %s.\n", path);
        return false;
    }

    cgltf_float matrix[16];
    cgltf_node_transform_world(node, matrix);
    if (node->mesh != NULL) {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++) {
            if (!appendPrimitive(model, vertexCapacity, indexCapacity,
                    batchCapacity, data, &node->mesh->primitives[i], matrix, path)) {
                return false;
            }
        }
    }
    for (cgltf_size i = 0; i < node->children_count; i++) {
        if (!appendNode(model, vertexCapacity, indexCapacity, batchCapacity,
                data, node->children[i], depth + 1, path)) {
            return false;
        }
    }
    return true;
}

static bool calculateBounds(Model *model) {
    if (model->vertexCount == 0) {
        return false;
    }

    float minX = model->vertices[0].x;
    float maxX = minX;
    float minY = model->vertices[0].y;
    float minZ = model->vertices[0].z;
    float maxZ = minZ;
    for (size_t i = 1; i < model->vertexCount; i++) {
        const ModelVertex *vertex = &model->vertices[i];
        if (vertex->x < minX) minX = vertex->x;
        if (vertex->x > maxX) maxX = vertex->x;
        if (vertex->y < minY) minY = vertex->y;
        if (vertex->z < minZ) minZ = vertex->z;
        if (vertex->z > maxZ) maxZ = vertex->z;
    }
    model->centerX = (minX + maxX) * 0.5f;
    model->centerZ = (minZ + maxZ) * 0.5f;
    model->minY = minY;
    return true;
}

bool gltfModelLoad(Model *model, const char *path) {
    if (model == NULL || path == NULL) {
        fprintf(stderr, "Invalid glTF model load arguments.\n");
        return false;
    }

    cgltf_options options = {0};
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        fprintf(stderr, "Could not parse glTF model %s (error %d).\n",
            path, (int)result);
        return false;
    }
    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        fprintf(stderr, "Could not load glTF buffers for %s (error %d).\n",
            path, (int)result);
        cgltf_free(data);
        return false;
    }
    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        fprintf(stderr, "Invalid glTF model %s (error %d).\n",
            path, (int)result);
        cgltf_free(data);
        return false;
    }

    Model loaded = {0};
    size_t vertexCapacity = 0;
    size_t indexCapacity = 0;
    size_t batchCapacity = 0;
    bool valid = loadMaterials(&loaded, data, path);

    if (valid && data->scene != NULL) {
        for (cgltf_size i = 0; valid && i < data->scene->nodes_count; i++) {
            valid = appendNode(&loaded, &vertexCapacity, &indexCapacity,
                &batchCapacity, data, data->scene->nodes[i], 0, path);
        }
    } else if (valid) {
        for (cgltf_size i = 0; valid && i < data->nodes_count; i++) {
            if (data->nodes[i].parent == NULL) {
                valid = appendNode(&loaded, &vertexCapacity, &indexCapacity,
                    &batchCapacity, data, &data->nodes[i], 0, path);
            }
        }
    }

    if (valid) {
        valid = calculateBounds(&loaded);
        if (!valid) {
            fprintf(stderr, "glTF model contains no triangle meshes: %s.\n", path);
        }
    }
    if (valid) {
        valid = modelPrepareRenderCache(&loaded);
    }
    cgltf_free(data);

    if (!valid) {
        modelDestroy(&loaded);
        return false;
    }
    modelDestroy(model);
    *model = loaded;
    return true;
}
