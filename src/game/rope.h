// The ropes of the ring. Each of the 4 sides has 3 ropes, and draws as 6
// pieces between 7 points.

#ifndef GAME_ROPE_H
#define GAME_ROPE_H

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

// A mesh that draws at a node: here one piece of a side. Its centre is in
// the space of its node. SceneObject of the scene's header at the merge.
typedef struct {
    u8 unknown_00[0x0C];
    float centre[3];
} RopeObject;

ULTRA_AT(RopeObject, centre, 0x0C);

// What a side draws: its pieces, and how far out its top rope is at each point.
typedef struct {
    RopeObject *piece[ROPE_PIECES];
    s16 out[ROPE_POINTS];
} RopeSide;

ULTRA_SIZE(RopeSide, 0x28);
ULTRA_AT(RopeSide, out, 0x18);

// Writes the vertices of the 3 ropes of a side, and the centre of each piece.
void rope_side(RopeVertex *vertex, int side);

#endif
