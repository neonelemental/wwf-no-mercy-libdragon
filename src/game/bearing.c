#include "bearing.h"

extern inline s32 turn_units(float radians);
extern inline s32 flat_turn(float y, float x);
extern inline float flat_length(float x, float y);
extern inline void point_way(const float *from, const float *to, float *way);
extern inline s32 way_quarter(const float *way);
extern inline s32 heading_to(const float *from, const float *to);
extern inline float distance_to(const float *from, const float *to);

float bearing_between(const float *from, const float *to, Bearing *out)
{
    Bearing b;
    point_way(from, to, b.way);
    b.length[0] = flat_length(b.way[1], b.way[2]);
    b.length[1] = flat_length(b.way[0], b.way[2]);
    b.length[2] = flat_length(b.way[0], b.way[1]);
    b.angle[0] = (float)flat_turn(b.way[2], b.way[1]);
    b.angle[1] = (float)flat_turn(b.way[0], b.way[2]);
    b.angle[2] = (float)flat_turn(b.way[0], b.way[1]);
    if (out) {
        *out = b;
    }
    return b.length[1];
}
