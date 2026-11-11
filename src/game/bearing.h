// The way from one point to another: its length and its angle, in the three
// planes or in the plane of the floor alone.

#ifndef GAME_BEARING_H
#define GAME_BEARING_H

#include "angle.h"
#include "ultra.h"

// What bearing_between gives of two points. Part n of length and of angle is
// in the plane across axis n, and an angle is whole, 4096 to a turn.
typedef struct {
    float way[3];
    float length[3];
    float angle[3];
} Bearing;

ULTRA_SIZE(Bearing, 0x24);

// An angle in radians as 4096 to a turn, cut to a whole number.
inline s32 turn_units(float radians)
{
    return (s32)((double)radians * 0x1.45f306dc9c883p+9);
}

// The angle of the point (x, y), 4096 to a turn.
inline s32 flat_turn(float y, float x)
{
    return turn_units(game_atan2f(y, x));
}

inline float flat_length(float x, float y)
{
    return __builtin_sqrtf(x * x + y * y);
}

inline void point_way(const float *from, const float *to, float *way)
{
    way[0] = to[0] - from[0];
    way[1] = to[1] - from[1];
    way[2] = to[2] - from[2];
}

// Which quarter of a turn about y a way points in: 0 within an eighth of a
// turn of z, and on from there.
inline s32 way_quarter(const float *way)
{
    return (flat_turn(way[0], way[2]) + 0x1200) / 0x400 & 3;
}

// The angle about y of the way from one point to another, and its length
// with no y.
inline s32 heading_to(const float *from, const float *to)
{
    float way[3];
    point_way(from, to, way);
    return flat_turn(way[0], way[2]);
}

inline float distance_to(const float *from, const float *to)
{
    float way[3];
    point_way(from, to, way);
    return flat_length(way[0], way[2]);
}

// Fills out, when it is not NULL, and gives the length with no y.
float bearing_between(const float *from, const float *to, Bearing *out);

#endif
