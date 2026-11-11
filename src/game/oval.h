// The ground that a wrestler takes: an oval on the floor.

#ifndef GAME_OVAL_H
#define GAME_OVAL_H

#include "model.h"

typedef struct {
    float half[3];    // half its size across and along its angle; no y
    float centre[3];
    s32 angle;        // about y, 4096 to a turn
} Oval;

ULTRA_SIZE(Oval, 0x1C);
ULTRA_AT(Oval, centre, 0x0C);
ULTRA_AT(Oval, angle, 0x18);

// The oval of a wrestler from the points of its joints: 36 across, and along
// the longest of four lines through the body, at least 29.7. For a wrestler
// on a wall, that wall is the floor.
void fighter_oval(Fighter *fighter, Oval *oval);
// How far an oval reaches from its centre toward a point. A half size of 0
// becomes 0.1.
float oval_reach(Oval *oval, const float *to);

// A way turned about y by an angle, added to a place when place is not NULL.
// It gives x and z, and leaves the y of out as it is.
void way_turned(const float *place, const float *way, s32 angle, float *out);

#endif
