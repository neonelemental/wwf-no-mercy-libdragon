#include "slot.h"

#include "angle.h"
#include "gu.h"

extern u32 D_80097EC0;  // bit 0x10000: a cage match
extern s16 D_800A8968;  // the place of the match
extern u8 game_mode;

// The shadow of each bone, in the game's data: 0 for none, or where its
// other end is; its width, which is also its least length, in 64ths; and how
// long it is for the length of the bone.
extern u8 D_8004804C[POSE_JOINTS];
extern u8 D_80048074[POSE_JOINTS];
extern float D_80048088[POSE_JOINTS];
// Not 0 for a slot that casts shadows.
extern u8 D_80076260[MODEL_SLOTS];

// Routines of the game that are still assembly.
void func_800151A0(SceneNode *node, float x, float y, float z);  // the draw pass of a node whose bone is there
void func_800111F4(Matrix m, float x, float y, float z, float *out_x, float *out_z);  // a point through a matrix, less its y
s32 func_8001EB04(void);
float func_8012DDD8(float x, float z, void *found);              // the height of the place's ground there, or 0

// The game's absolute value, which keeps a zero's sign.
static inline float absf(float x)
{
    return (double)x < 0.0 ? -x : x;
}

// A float of a joint that model.h has no name for.
static inline float joint_float(const Joint *joint, int at)
{
    float f;
    __builtin_memcpy(&f, (const u8 *)joint + at, sizeof f);
    return f;
}

void slot_passes(s32 slot)
{
    SceneNode *nodes = D_80091CC0[(s16)slot];
    Joint *joints = D_800A1130[(s16)slot];
    if (nodes[1].pass & 0x80) {
        return;
    }
    float *first = joints[1].world;
    // A wrestler up on a wall of the cage draws in one pass, all of it.
    if ((D_80097EC0 & 0x10000) && 90.0 <= (double)first[1]
        && (270.0 < (double)absf(first[2]) || 280.0 < (double)absf(first[0]))) {
        u8 pass = (double)first[2] < -270.0 ? 5 : 0;
        for (int n = 1; n < POSE_JOINTS; n++) {
            nodes[n].pass = pass;
        }
    } else {
        for (int n = 1; n < POSE_JOINTS; n++) {
            float *at = joints[n].world;
            func_800151A0(&nodes[n], -at[0], at[1], at[2]);
        }
    }
    // The nodes past those of a pose take the pass of the node they go with.
    u8 pass1 = nodes[1].pass, pass2 = nodes[2].pass, pass3 = nodes[3].pass, pass5 = nodes[5].pass;
    nodes[4].pass = 0x80;
    nodes[22].pass = pass1;
    nodes[23].pass = pass2;
    nodes[24].pass = pass3;
    nodes[20].pass = pass5;
    nodes[21].pass = pass5;
    for (int n = 25; n < MODEL_NODES; n++) {
        if (nodes[n].parent) {
            nodes[n].pass = nodes[n].parent->pass;
        }
    }
}

s32 over_ring(float x, float z)
{
    u8 mode = game_mode;
    if (!((double)absf(x) < 280.0) || !((double)absf(z) < 280.0)) {
        return 0;
    }
    if (D_800A8968 < 10) {
        return 1;
    }
    if (mode == 3) {
        return (s16)func_8001EB04() != 0;
    }
    return D_800A8968 == 30;
}

void slot_shadows(SceneNode *shadows, s32 slot, float ring, float floor)
{
    slot = (s16)slot;
    SceneNode *nodes = D_80091CC0[slot];
    Joint *joints = D_800A1130[slot];
    // The other end of the bone, then the way to it from the first.
    float x = 0, z = 0;
    for (int n = 1; n < POSE_JOINTS; n++) {
        SceneNode *shadow = &shadows[n];
        Joint *joint = &joints[n];
        u8 kind = D_8004804C[n];
        if (kind == 0 || D_80076260[slot] == 0) {
            shadow->pass |= 0x80;
            continue;
        }
        switch (kind) {
        case 1:
            func_800111F4(nodes[n].world, joint_float(joint, 0x30), joint->point[1], joint->point[2], &x, &z);
            x = -x;
            break;
        case 2:
            x = joint->world2[0];
            z = joint->world2[2];
            break;
        case 3:
            func_800111F4(nodes[n].world, joint->point[0], joint->point[1], joint_float(joint, 0x38), &x, &z);
            x = -x;
            break;
        case 4:
            x = joints[3].world2[0];
            z = joints[3].world2[2];
            break;
        }
        z = z - joint->world[2];
        x = x - joint->world[0];
        float turn = (float)((double)game_atan2f(z, x) * 57.2957795130823229);
        u8 width = D_80048074[n];
        u8 length = float_to_u32(__builtin_sqrtf(x * x + z * z) * D_80048088[n]);
        if (length < width) {
            length = width;
        }

        u8 mode = game_mode;
        float at_x = joint->world[0], at_z = joint->world[2];
        float y;
        if (mode == 1) {
            // Beside the ring, ground that is not at 0 takes the shadow at the ring's height.
            if ((s16)over_ring(at_x, at_z)) {
                y = ring;
            } else {
                y = func_8012DDD8(at_x, at_z, NULL) == 0.0f ? floor : ring;
            }
        } else if (mode == 2) {
            y = (s16)over_ring(at_x, at_z) ? ring : floor;
        } else {
            y = floor;
            if ((s16)over_ring(at_x, at_z) && ring - 20.0f <= joint->world[1]) {
                y = ring;
            }
        }

        shadow->pass = nodes[n].pass;
        Matrix scale, turned;
        guScaleF(scale, (float)((double)(float)length * 0.015625), 1.0f, (float)((double)(float)width * 0.015625));
        mtx_rotate_y(turned, turn);
        mtx_mul(scale, turned, shadow->local);
        shadow->local[3][0] = -joint->world[0];
        shadow->local[3][1] = y;
        shadow->local[3][2] = joint->world[2];
    }
}
