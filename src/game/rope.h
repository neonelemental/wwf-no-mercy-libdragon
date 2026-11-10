// The ring of a match: its ropes, its bounce, the cage about it and the
// things beside it. Each of the 4 sides has 3 ropes, and draws as 6 pieces
// between 7 points.

#ifndef GAME_ROPE_H
#define GAME_ROPE_H

#include "scene.h"
#include "ultra.h"

#define ROPE_SIDES 4
#define ROPE_ROPES 3
#define ROPE_POINTS 7
#define ROPE_PIECES 6

// One rope of a side. It bends up and out the most at one place along the
// side, and less from there to each post. A bend swings to its end by a step
// each tick, and the end then turns about and gets shorter.
typedef struct {
    s16 state;  // 0 at rest; 7 swings slower and longer
    s16 ticks;  // until the swing turns
    float at;
    float up;
    float out;
    u8 unknown_10[4];
    float up_end;
    float out_end;
    u8 unknown_1C[4];
    float up_step;
    float out_step;
} Rope;

ULTRA_SIZE(Rope, 0x28);
ULTRA_AT(Rope, ticks, 0x02);
ULTRA_AT(Rope, at, 0x04);
ULTRA_AT(Rope, up, 0x08);
ULTRA_AT(Rope, out, 0x0C);
ULTRA_AT(Rope, up_end, 0x14);
ULTRA_AT(Rope, out_end, 0x18);
ULTRA_AT(Rope, up_step, 0x20);
ULTRA_AT(Rope, out_step, 0x24);

// What a side draws: its pieces, and how far out its top rope is at each point.
typedef struct {
    SceneObject *piece[ROPE_PIECES];
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

// The tick of the place of a match: the ring, or another place.
void arena_tick(void);
// Swings each rope and writes each side.
void rope_swing(void);
// Writes the vertices of the 3 ropes of a side, and the centre of each piece.
void rope_side(Vertex *vertex, int side);
// Bounces the ring, sorts and fades what stands about it, and walks its nodes.
void ring_tick(void);
// Breaks the thing of 4 parts, and sorts it and the 2 things of 3 nodes.
void ringside_tick(void);

#endif
