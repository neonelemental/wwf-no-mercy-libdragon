// Where the head of a wrestler turns to, to look at its opponent.

#ifndef GAME_LOOK_H
#define GAME_LOOK_H

#include "ultra.h"

// Moves look[1], the turn of the head about y, and look[0], its tilt, toward
// the head of the opponent: both in degrees, at most 8 and 4 a tick, and
// within 67.5 and 30. Gives 0, and no look, for a wrestler that does not
// look at its opponent.
s32 fighter_look(s32 slot, float *look);

#endif
