// The passes of a match over the model of each slot: the draw pass of its
// nodes, and the shadow that each bone casts.

#ifndef GAME_SLOT_H
#define GAME_SLOT_H

#include "model.h"

// Gives each node of the slot's model its draw pass, by where its bone is.
void slot_passes(s32 slot);

#endif
