#include "model.h"

#include "gu.h"

// An angle of the animation data, 1024 to a turn, in degrees.
static const double ANGLE = 360.0 / 1024;

// The last place that a pose gave whole. A short place is a step from it.
extern float D_80069F10[3];

// The joints of a pose, in the game's data: where each is at rest in x and
// y, the order to make them in, the parent of each or 0 for a joint with a
// place of its own, and 0 or 1 for a hand or -1.
extern s8 D_80047ED0[POSE_JOINTS][2];
extern s8 D_80047EF8[POSE_JOINTS];
extern s8 D_80047F0C[POSE_JOINTS];
extern s8 D_80047F40[POSE_JOINTS];
// The kind of hands of each slot, and for each kind their scale and where
// their pivot is.
extern s8 D_8006A268[];
extern float D_80047F54[], D_80047F68[], D_80047F7C[];
extern float *D_80047FE0[2];
extern float D_80047FB8[], D_80047FCC[];
// The share of the way to the next pose that a step moves.
extern float D_8004800C[3];

// Routines of the game that are still assembly.
void func_80010FC4(Matrix m, float x, float y, float z);        // sets the place of a matrix
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

void joint_chain(s32 slot_word)
{
    s16 slot = slot_word;
    Matrix bone[POSE_JOINTS];
    Matrix pivot;

    for (int n = 1; n < POSE_JOINTS; n++) {
        int j = D_80047EF8[n];
        ModelNode *node = &D_80091CC0[slot][j];
        Joint *joint = &D_800A1130[slot][j];
        int parent = D_80047F0C[j];
        if (parent != 0) {
            guMtxXFMF(bone[parent],
                (float)(D_80047ED0[j][0] - D_80047ED0[parent][0]),
                (float)(D_80047ED0[j][1] - D_80047ED0[parent][1]),
                0.0f, &joint->place[0], &joint->place[1], &joint->place[2]);
        }
        mtx_rotate(bone[j], joint->angle[0], joint->angle[1], joint->angle[2]);
        func_80010FC4(bone[j], joint->place[0], joint->place[1], joint->place[2]);
        if (D_80047F40[j] == -1) {
            guTranslateF(pivot, (float)-D_80047ED0[j][0], (float)-D_80047ED0[j][1], 0.0f);
        } else {
            int kind = D_8006A268[slot];
            guScaleF(pivot, D_80047F54[kind], D_80047F68[kind], D_80047F7C[kind]);
            int hand = D_80047F40[j];
            kind = D_8006A268[slot];
            float y = D_80047FB8[kind] - (float)D_80047ED0[j][1];
            float x = D_80047FE0[hand][kind] - (float)D_80047ED0[j][0];
            func_80010FC4(pivot, x, y, D_80047FCC[kind]);
        }
        mtx_mul(pivot, bone[j], node->local);
        float px = joint->place[0], py = joint->place[1], pz = joint->place[2];
        float ax = joint->angle[0], ay = joint->angle[1], az = joint->angle[2];
        joint->last_place[0] = px;
        joint->last_place[1] = py;
        joint->last_place[2] = pz;
        joint->last_angle[0] = ax;
        joint->last_angle[1] = ay;
        joint->last_angle[2] = az;
    }
}
