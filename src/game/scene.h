// The scene: the trees of nodes that a match places, and the passes over them.

#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "matrix.h"
#include "ultra.h"

// A node of a tree: a joint of a wrestler, a piece of a rope, a part of the set.
typedef struct SceneNode {
    struct SceneNode *parent;
    struct SceneNode *child;  // its first child
    struct SceneNode *next;   // the next child of its parent
    u8 unknown_0C[0x10];
    Matrix local;             // from its parent
    Matrix world;             // local by the world matrix of its parent
    Matrix product;           // world by the view-projection matrix
} SceneNode;

ULTRA_SIZE(SceneNode, 0xDC);
ULTRA_AT(SceneNode, parent, 0x00);
ULTRA_AT(SceneNode, child, 0x04);
ULTRA_AT(SceneNode, next, 0x08);
ULTRA_AT(SceneNode, local, 0x1C);
ULTRA_AT(SceneNode, world, 0x5C);
ULTRA_AT(SceneNode, product, 0x9C);

// Gives each node of a tree its world matrix, each parent before its children.
void scene_walk(SceneNode *node);

#endif
