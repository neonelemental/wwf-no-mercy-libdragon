// The ring of a match: its ropes, its bounce, the cage about it and the
// things beside it. Each of the 4 sides has 3 ropes, and draws as 6 pieces
// between 7 points.

#ifndef GAME_ROPE_H
#define GAME_ROPE_H

#include "matrix.h"
#include "ultra.h"

#define ROPE_SIDES 4
#define ROPE_ROPES 3
#define ROPE_POINTS 7
#define ROPE_PIECES 6

// One rope of a side. It bends up and out the most at one place along the
// side, and less from there to each post.
typedef struct {
    s16 state;
    s16 ticks;
    float at;
    float up;
    float out;
    u8 unknown_10[0x10];
    float up_step;
    float out_step;
} Rope;

ULTRA_SIZE(Rope, 0x28);
ULTRA_AT(Rope, ticks, 0x02);
ULTRA_AT(Rope, at, 0x04);
ULTRA_AT(Rope, up, 0x08);
ULTRA_AT(Rope, out, 0x0C);
ULTRA_AT(Rope, up_step, 0x20);
ULTRA_AT(Rope, out_step, 0x24);

// A vertex of a display list, as the RSP reads it. Vertex of the scene's
// header at the merge.
typedef struct {
    s16 x, y, z;
    u8 unknown_06[0x0A];
} RopeVertex;

ULTRA_SIZE(RopeVertex, 0x10);
ULTRA_AT(RopeVertex, y, 0x02);
ULTRA_AT(RopeVertex, z, 0x04);

// A mesh that draws at a node, such as one piece of a side. Its centre is in
// the space of its node, and the scene sorts by its key: its distance from
// the camera and its bias. SceneObject of the scene's header at the merge.
typedef struct {
    s32 kind;  // 1 draws only when it is in view
    u8 unknown_04[8];
    float centre[3];
    float projected[3];
    float bias;
    float key;
} RopeObject;

ULTRA_AT(RopeObject, centre, 0x0C);
ULTRA_AT(RopeObject, projected, 0x18);
ULTRA_AT(RopeObject, bias, 0x24);
ULTRA_AT(RopeObject, key, 0x28);

// A node of the scene. SceneNode of the scene's header at the merge.
typedef struct RopeNode {
    struct RopeNode *parent;
    struct RopeNode *child;
    struct RopeNode *next;
    RopeVertex *vertices;
    u8 unknown_10[4];
    RopeObject *objects;
    u8 pass;  // the draw pass of its objects
    Matrix local;
    u8 unknown_5C[0x80];
} RopeNode;

ULTRA_SIZE(RopeNode, 0xDC);
ULTRA_AT(RopeNode, vertices, 0x0C);
ULTRA_AT(RopeNode, objects, 0x14);
ULTRA_AT(RopeNode, pass, 0x18);
ULTRA_AT(RopeNode, local, 0x1C);

// What a side draws: its pieces, and how far out its top rope is at each point.
typedef struct {
    RopeObject *piece[ROPE_PIECES];
    s16 out[ROPE_POINTS];
} RopeSide;

ULTRA_SIZE(RopeSide, 0x28);
ULTRA_AT(RopeSide, out, 0x18);

// The ring. Its bounce swings to its end by a step each tick, and the end
// then turns about and gets shorter. Beside the ring stands a thing of 4
// parts that can break: it falls apart in steps, blinks and goes.
typedef struct {
    float bounce_end;
    float bounce;
    float bounce_step;
    u8 broken;  // 0 whole, 1 falling apart, 2 blinking, 3 gone
    u8 broken_step;
    u8 broken_slow;
    u8 broken_wait;
    u8 unknown_10[4];
    s16 shake;  // ticks that the cage still shakes
    u8 unknown_16[2];
    s16 place;
    u8 unknown_1A[2];
    s32 wall_solid;
    u32 ticks;
} Ring;

ULTRA_AT(Ring, broken, 0x0C);
ULTRA_AT(Ring, shake, 0x14);
ULTRA_AT(Ring, place, 0x18);
ULTRA_AT(Ring, wall_solid, 0x1C);
ULTRA_AT(Ring, ticks, 0x20);

// Writes the vertices of the 3 ropes of a side, and the centre of each piece.
void rope_side(RopeVertex *vertex, int side);
// Bounces the ring, sorts and fades what stands about it, and walks its nodes.
void ring_tick(void);
// Breaks the thing of 4 parts, and sorts it and the 2 things of 3 nodes.
void ringside_tick(void);

#endif
