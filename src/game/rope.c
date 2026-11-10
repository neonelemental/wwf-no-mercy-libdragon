#include "rope.h"

extern Rope D_80084410[ROPE_SIDES][ROPE_ROPES];
extern RopeSide D_80084370[ROPE_SIDES];
// The height of each rope of a side, and the 4 corners of a rope's section.
extern u8 D_80052888[ROPE_ROPES];
extern s8 D_80052848[4];
extern s8 D_8005284C[4];

// The game's float to u32.
static inline u32 to_u32(float x)
{
    if (2147483648.0f <= x) {
        return (u32)(s32)(x - 2147483648.0f) | 0x80000000;
    }
    return (u32)(s32)x;
}

void rope_side(RopeVertex *vertex, int side)
{
    for (int point = 0; point < ROPE_POINTS; point++) {
        float x = (float)(point * 74 - 224);
        float out = 0;
        for (int i = 0; i < ROPE_ROPES; i++) {
            Rope *rope = &D_80084410[side][i];
            float at = rope->at;
            float part;
            if (x < at) {
                part = (x - -224.0f) / (at - -224.0f);
            } else {
                part = (224.0f - x) / (224.0f - at);
            }
            float up = rope->up * part;
            out = rope->out * part;
            up = up + (float)D_80052888[i];
            for (int corner = 0; corner < 4; corner++, vertex++) {
                vertex->y = (s32)(up + (float)D_80052848[corner]);
                vertex->z = (s32)(out + (float)D_8005284C[corner]);
            }
        }
        D_80084370[side].out[point] = to_u32(out);
    }
    RopeSide *drawn = &D_80084370[side];
    for (int i = 0; i < ROPE_PIECES; i++) {
        s16 a = drawn->out[i], b = drawn->out[i + 1];
        drawn->piece[i]->centre[2] = (float)(a < b ? b : a);
    }
}
