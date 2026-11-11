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

#endif
