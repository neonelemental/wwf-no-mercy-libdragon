#include "slot.h"

extern u32 D_80097EC0;  // bit 0x10000: a cage match

// Routines of the game that are still assembly.
void func_800151A0(SceneNode *node, float x, float y, float z);  // the draw pass of a node whose bone is there

// The game's absolute value, which keeps a zero's sign.
static inline float absf(float x)
{
    return (double)x < 0.0 ? -x : x;
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
