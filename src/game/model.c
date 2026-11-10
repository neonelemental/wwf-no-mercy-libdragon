#include "model.h"

// An angle of the animation data, 1024 to a turn, in degrees.
static const double ANGLE = 360.0 / 1024;

// The last place that a pose gave whole. A short place is a step from it.
extern float D_80069F10[3];

// The joints of a pose, in the game's data: the order to make them in, and
// the parent of each or 0 for a joint with a place of its own.
extern s8 D_80047EF8[POSE_JOINTS];
extern s8 D_80047F0C[POSE_JOINTS];
// The share of the way to the next pose that a step moves.
extern float D_8004800C[3];

// Routines of the game that are still assembly.
float func_80011270(float degrees);                              // brings an angle into -180 to 180
float func_80011324(float from, float to);                       // the way from one angle to another
float func_8000567C(float x);                                    // its size
void func_80012B24(s32 slot);                                    // takes the next pose from the cartridge

static u32 word(const u8 *p)
{
    return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

// A sign bit above the bits of a size.
static int signed_size(u32 v, u32 sign)
{
    return v & sign ? -(int)(v & (sign - 1)) : (int)(v & (sign - 1));
}

u8 *pose_read(u8 *data, float angle[3], float place[3])
{
    u32 w = word(data);
    data += 4;
    float x = (float)((double)(float)(int)(w & 0x3FF) * ANGLE);
    float y = (float)((double)(float)(int)(w >> 10 & 0x3FF) * ANGLE);
    float z = (float)((double)(float)(int)(w >> 20 & 0x3FF) * ANGLE);
    angle[0] = x;
    angle[1] = y;
    angle[2] = z;
    if (!(w & 0x80000000)) {
        return data;
    }
    if (w & 0x40000000) {
        // Three bytes, each a step in halves.
        int sx = signed_size(data[0], 0x80);
        int sy = signed_size(data[1], 0x80);
        int sz = signed_size(data[2], 0x80);
        data += 3;
        place[0] = (float)((double)(float)sx * 0.5) + D_80069F10[0];
        place[1] = (float)((double)(float)sy * 0.5) + D_80069F10[1];
        place[2] = (float)((double)(float)sz * 0.5) + D_80069F10[2];
    } else {
        // A word of 11, 11 and 10 bits.
        u32 p = word(data);
        data += 4;
        place[0] = (float)signed_size(p, 0x400);
        place[1] = (float)signed_size(p >> 11, 0x400);
        place[2] = (float)signed_size(p >> 22, 0x200);
        D_80069F10[0] = place[0];
        D_80069F10[1] = place[1];
        D_80069F10[2] = place[2];
    }
    return data;
}

void anim_read(s32 slot_word, s32 step_word)
{
    s16 slot = slot_word;
    int step = (s16)step_word % 3;
    float place[POSE_JOINTS][3];
    float angle[POSE_JOINTS][3];

    func_80012B24(slot);
    Animation *anim = &D_80069F20[slot];
    u8 *data = anim->data[anim->now];
    for (int n = 1; n < POSE_JOINTS; n++) {
        data = pose_read(data, angle[n], place[n]);
    }
    float *share = &D_8004800C[step];
    for (int n = 1; n < POSE_JOINTS; n++) {
        int j = D_80047EF8[n];
        Joint *joint = &D_800A1130[slot][j];
        float *turn = angle[j];
        if (D_80047F0C[j] == 0) {
            joint->place[0] = joint->place[0] + (place[j][0] - joint->place[0]) * *share;
            joint->place[1] = joint->place[1] + (place[j][1] - joint->place[1]) * *share;
            joint->place[2] = joint->place[2] + (place[j][2] - joint->place[2]) * *share;
        }
        turn[0] = func_80011324(joint->angle[0], turn[0]);
        turn[1] = func_80011324(joint->angle[1], turn[1]);
        turn[2] = func_80011324(joint->angle[2], turn[2]);
        // Past 120 about x or z, both go by 180 less.
        if (120.0 < (double)func_8000567C(turn[0]) || 120.0 < (double)func_8000567C(turn[2])) {
            turn[0] = func_80011270(turn[0] + 180.0f);
            turn[2] = func_80011270(turn[2] + 180.0f);
        }
        joint->angle[0] = joint->angle[0] + turn[0] * *share;
        joint->angle[1] = joint->angle[1] + turn[1] * *share;
        joint->angle[2] = joint->angle[2] + turn[2] * *share;
    }
}
