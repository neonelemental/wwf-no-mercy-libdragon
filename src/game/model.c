#include "model.h"

// An angle of the animation data, 1024 to a turn, in degrees.
static const double ANGLE = 360.0 / 1024;

// The last place that a pose gave whole. A short place is a step from it.
extern float D_80069F10[3];

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
