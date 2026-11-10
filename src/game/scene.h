// The scene: the trees of nodes that a match places, the objects that draw
// at them, and the passes over both.

#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include "gu.h"
#include "matrix.h"

// A vertex as the RSP reads it, with a normal in place of a colour.
typedef struct {
    s16 x, y, z;
    u16 flag;
    s16 s, t;
    s8 normal[3];
    u8 alpha;
} Vertex;

struct SceneObject;

// A node of a tree: a joint of a wrestler, a piece of a rope, a part of the set.
typedef struct SceneNode {
    struct SceneNode *parent;
    struct SceneNode *child;      // its first child
    struct SceneNode *next;       // the next child of its parent
    Vertex *vertices;             // the centre of its objects is the mean of the first `count`
    void *list;                   // the display list that its objects draw
    struct SceneObject *objects;  // the first of its chain of objects
    u8 pass;                      // the draw pass of its objects; with bit 7, the product pass leaves its product
    u8 raw;                       // not 0: the product pass leaves its product, and its lists get no colours
    u8 colour;                    // which PRIM and ENV colours its lists get; the light pass leaves 0 dark
    s8 count;
    Matrix local;                 // from its parent
    Matrix world;                 // local by the world matrix of its parent
    Matrix product;               // world by the view-projection matrix
} SceneNode;

// A mesh. It draws by the matrices of its node, which is not always the node
// whose chain it is in.
typedef struct SceneObject {
    s32 kind;                  // 1: drawn only when its centre is in view
    SceneNode *node;
    struct SceneObject *next;  // the next object of the chain
    float centre[3];           // in the space of its node
    float projected[3];        // its centre through the product of its node
    float bias;                // added to its depth
    float key;                 // the depth sort puts the largest first
    s16 count;                 // of its vertices
    s16 first;                 // the place of its first vertex in the RSP's buffer
    Vertex *vertices;
    void *list;                // its display list
    Mtx mtx[2];                // the product of its node, for each of the two frames in turn
} SceneObject;

ULTRA_SIZE(Vertex, 0x10);
ULTRA_AT(Vertex, normal, 0x0C);
ULTRA_AT(Vertex, alpha, 0x0F);

ULTRA_SIZE(SceneNode, 0xDC);
ULTRA_AT(SceneNode, parent, 0x00);
ULTRA_AT(SceneNode, child, 0x04);
ULTRA_AT(SceneNode, next, 0x08);
ULTRA_AT(SceneNode, vertices, 0x0C);
ULTRA_AT(SceneNode, list, 0x10);
ULTRA_AT(SceneNode, objects, 0x14);
ULTRA_AT(SceneNode, pass, 0x18);
ULTRA_AT(SceneNode, raw, 0x19);
ULTRA_AT(SceneNode, colour, 0x1A);
ULTRA_AT(SceneNode, count, 0x1B);
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
ULTRA_AT(SceneObject, count, 0x2C);
ULTRA_AT(SceneObject, first, 0x2E);
ULTRA_AT(SceneObject, vertices, 0x30);
ULTRA_AT(SceneObject, list, 0x34);
ULTRA_AT(SceneObject, mtx, 0x38);

// Gives each node of a tree its world matrix, each parent before its children.
void scene_walk(SceneNode *node);
// Gives the node of each object its product, and the object its depth key.
void scene_project(void);
// Puts the objects in the order of their keys, the largest first. When whole
// is 0, it makes one pass that swaps neighbours.
void scene_sort(s32 whole);
// Shades the vertices of a tree by a light at a point: each alpha is base,
// plus gain by how far the vertex faces the light, held to 255.
void scene_light(SceneNode *tree, float *light, u32 gain, u32 base);
// The same for a tree and the nodes after it, by the light of the last scene_light.
void scene_light_walk(SceneNode *node);

#endif
