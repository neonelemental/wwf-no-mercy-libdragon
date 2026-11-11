// What is near a wrestler, and the push that keeps two of them apart.

#ifndef GAME_PAIR_H
#define GAME_PAIR_H

#include "oval.h"

// A wrestler or a thing near a wrestler.
typedef struct {
    s32 who;          // a slot, or 100 and the number of a thing; -1 for none
    s32 angle;        // about y, from the wrestler to it
    float distance;
    Oval oval;
} Near;

ULTRA_SIZE(Near, 0x28);
ULTRA_AT(Near, oval, 0x0C);

#define NEAR_MAX 10

// Fills list[NEAR_MAX] with the oval of the wrestler, and the entries before
// it with each other wrestler and thing that it can touch, the nearest
// first. An entry that is not used has who -1.
void pair_list(Fighter *fighter, Near *list);

// Moves the wrestler, and second when it is not NULL, out of each oval that
// its own lies over, by the depth over BY. Leaves out the entry SKIP. AXES
// has bits 8 and 9 for a push in x and bits 10 and 11 for one in z, and 0 is
// both. MODE 0 moves nothing, and 2 pushes an eighth of a turn to the side.
// Gives the nearest that it lay over, or -1.
s32 pair_push(Fighter *fighter, Fighter *second, float by, s32 axes, s32 skip, s32 mode);

#endif
