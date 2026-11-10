#include "scene.h"

#include <string.h>

void scene_walk(SceneNode *node)
{
    for (; node; node = node->next) {
        if (!node->parent) {
            memcpy(node->world, node->local, sizeof(Matrix));
        } else {
            mtx_mul(node->local, node->parent->world, node->world);
        }
        if (node->child) {
            scene_walk(node->child);
        }
    }
}

// The first object of each node that draws, and their count.
extern SceneObject *D_80060BA0[900];
extern s16 D_800619B2;
// The view matrix by the projection matrix.
extern Matrix D_80062BA8;

void scene_project(void)
{
    for (u32 i = 0; i < (u32)D_800619B2; i++) {
        SceneObject *object = D_80060BA0[i];
        SceneNode *node = object->node;
        if ((node->pass & 0x80) || node->raw) {
            object->key = (float)((double)object->bias * 1024.0);
            continue;
        }
        mtx_mul(node->world, D_80062BA8, node->product);
        guMtxXFMF(node->product, object->centre[0], object->centre[1], object->centre[2],
            &object->projected[0], &object->projected[1], &object->projected[2]);
        float xx = object->projected[0] * object->projected[0];
        float yy = object->projected[1] * object->projected[1];
        object->key = (float)((double)(object->bias + object->projected[2]) * 1024.0);
        object->key = object->key + __builtin_sqrtf(xx + yy);
    }
}

void scene_sort(s32 whole)
{
    SceneObject **table = D_80060BA0;
    u32 count = (u32)D_800619B2;
    if ((s16)whole) {
        for (u32 i = 1; i < count; i++) {
            SceneObject *object = table[i];
            u32 j = i;
            while (j && table[j - 1]->key < object->key) {
                table[j] = table[j - 1];
                j--;
            }
            table[j] = object;
        }
    } else {
        for (u32 i = 1; i < count; i++) {
            SceneObject *a = table[i - 1], *b = table[i];
            if (a->key < b->key) {
                table[i] = a;
                table[i - 1] = b;
            }
        }
    }
}

// The light of the light pass: its gain, its base and its place.
extern float D_80062B50;
extern float D_80062B54;
extern float D_80062B58;
extern float D_80062B5C;
extern float D_80062B60;

// A float as an unsigned word, the way the game's compiler converts one.
static inline u32 float_to_u32(float f)
{
    if (2147483648.0f <= f) {
        return (u32)(s32)(f - 2147483648.0f) | 0x80000000;
    }
    return (s32)f;
}

void scene_light_walk(SceneNode *node)
{
    float to_light[3], origin[3], normal[3];
    for (; node; node = node->next) {
        SceneObject *object = node->objects;
        if (object && node->colour) {
            guMtxXFMF(node->world, object->centre[0], object->centre[1], object->centre[2],
                &to_light[0], &to_light[1], &to_light[2]);
            to_light[0] = D_80062B58 - to_light[0];
            to_light[1] = D_80062B5C - to_light[1];
            to_light[2] = D_80062B60 - to_light[2];
            guNormalize(&to_light[0], &to_light[1], &to_light[2]);
            do {
                guMtxXFMF(object->node->world, 0, 0, 0, &origin[0], &origin[1], &origin[2]);
                Vertex *vertex = object->vertices;
                for (u32 i = 0; i < (u32)object->count; i++, vertex++) {
                    guMtxXFMF(object->node->world, vertex->normal[0], vertex->normal[1], vertex->normal[2],
                        &normal[0], &normal[1], &normal[2]);
                    normal[0] -= origin[0];
                    normal[1] -= origin[1];
                    normal[2] -= origin[2];
                    guNormalize(&normal[0], &normal[1], &normal[2]);
                    float facing = to_light[0] * normal[0] + to_light[1] * normal[1] + to_light[2] * normal[2];
                    float shade = D_80062B54;
                    if (0 < facing) {
                        shade = shade + D_80062B50 * facing;
                        if (255.0 < (double)shade) {
                            shade = 255;
                        }
                    }
                    vertex->alpha = float_to_u32(shade);
                }
                object = object->next;
            } while (object);
        }
        if (node->child) {
            scene_light_walk(node->child);
        }
    }
}

void scene_light(SceneNode *tree, float *light, u32 gain, u32 base)
{
    float x = light[0], y = light[1], z = light[2];
    D_80062B50 = (s32)(gain & 0xFF);
    D_80062B54 = (s32)(base & 0xFF);
    D_80062B58 = x;
    D_80062B5C = y;
    D_80062B60 = z;
    scene_light_walk(tree);
}
