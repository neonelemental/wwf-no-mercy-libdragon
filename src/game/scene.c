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
