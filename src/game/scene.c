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
