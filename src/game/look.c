#include "look.h"

#include "bearing.h"
#include "model.h"

// An angle of 4096 to a turn, in degrees.
static const double DEGREES = 360.0 / 4096;

// A step toward WAY of at most LIMIT.
static float step(float way, float limit)
{
    float by = way;
    if ((double)limit < (double)way) {
        by = limit;
    }
    if ((double)by < -(double)limit) {
        by = -limit;
    }
    return by;
}

s32 fighter_look(s32 slot, float *look)
{
    Fighter *fighter = &D_80159990_ovl_e[slot];
    if (!(fighter->flags8A & 0x100)) {
        look[0] = 0.0f;
        look[1] = 0.0f;
        look[2] = 0.0f;
        return 0;
    }
    Bearing b;
    bearing_between(fighter->point[5], D_80159990_ovl_e[fighter->opponent].point[5], &b);

    float turn = (float)((double)turn_between((s32)b.angle[1], fighter->angle[1]) * DEGREES);
    look[1] = look[1] + step(turn, 8.0f);
    if (flat_absf(turn) < flat_absf(look[1])) {
        look[1] = turn;
    }
    if (67.5 < (double)look[1]) {
        look[1] = 67.5f;
    }
    if ((double)look[1] < -67.5) {
        look[1] = -67.5f;
    }
    if (!(fighter->flags & 0xA) && fighter->file != 0x2F3C) {
        look[1] = 0.0f;
    }

    s32 up = turn_between(flat_turn((float)((double)b.length[1] * 1.25), b.way[1]), 0x400);
    float tilt = (float)-((double)up * DEGREES);
    look[0] = look[0] + step(tilt, 4.0f);
    if (flat_absf(tilt) < flat_absf(look[0])) {
        look[0] = tilt;
    }
    if (30.0 < (double)look[0]) {
        look[0] = 30.0f;
    }
    if ((double)look[0] < -30.0) {
        look[0] = -30.0f;
    }
    return 1;
}
