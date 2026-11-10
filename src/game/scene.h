// The scene: the trees of nodes that a match places, the objects that draw
// at them, and the passes over both.

#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "gu.h"
#include "matrix.h"

struct SceneObject;

// A node of a tree: a joint of a wrestler, a piece of a rope, a part of the set.
typedef struct SceneNode {
    struct SceneNode *parent;
    struct SceneNode *child;      // its first child
    struct SceneNode *next;       // the next child of its parent
    u8 unknown_0C[8];
    struct SceneObject *objects;  // its first object
    u8 pass;                      // the draw pass of its objects; with bit 7, the product pass leaves its product
    u8 raw;                       // not 0: the product pass leaves its product
    u8 unknown_1A[2];
    Matrix local;                 // from its parent
    Matrix world;                 // local by the world matrix of its parent
    Matrix product;               // world by the view-projection matrix
} SceneNode;

// A mesh that draws at a node.
typedef struct SceneObject {
    s32 kind;
    SceneNode *node;
    struct SceneObject *next;  // the next object of its node
    float centre[3];           // in the space of its node
    float projected[3];        // its centre through the product of its node
    float bias;                // added to its depth
    float key;                 // the depth sort puts the largest first
    u8 unknown_2C[0x0C];
    Mtx mtx[2];                // the product of its node, for each of the two frames in turn
} SceneObject;

ULTRA_SIZE(SceneNode, 0xDC);
ULTRA_AT(SceneNode, parent, 0x00);
ULTRA_AT(SceneNode, child, 0x04);
ULTRA_AT(SceneNode, next, 0x08);
ULTRA_AT(SceneNode, objects, 0x14);
ULTRA_AT(SceneNode, pass, 0x18);
ULTRA_AT(SceneNode, raw, 0x19);
ULTRA_AT(SceneNode, local, 0x1C);
ULTRA_AT(SceneNode, world, 0x5C);
ULTRA_AT(SceneNode, product, 0x9C);

ULTRA_SIZE(SceneObject, 0xB8);
ULTRA_AT(SceneObject, kind, 0x00);
ULTRA_AT(SceneObject, node, 0x04);
ULTRA_AT(SceneObject, next, 0x08);
ULTRA_AT(SceneObject, centre, 0x0C);
ULTRA_AT(SceneObject, projected, 0x18);
ULTRA_AT(SceneObject, bias, 0x24);
ULTRA_AT(SceneObject, key, 0x28);
ULTRA_AT(SceneObject, mtx, 0x38);

// Gives each node of a tree its world matrix, each parent before its children.
void scene_walk(SceneNode *node);
// Gives the node of each object its product, and the object its depth key.
void scene_project(void);
// Puts the objects in the order of their keys, the largest first. When whole
// is 0, it makes one pass that swaps neighbours.
void scene_sort(s32 whole);

#endif
