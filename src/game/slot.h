// The passes of a match over the model of each slot: the draw pass of its
// nodes, and the shadow that each bone casts.

#ifndef GAME_SLOT_H
#define GAME_SLOT_H

#include "model.h"

// Gives each node of the slot's model its draw pass, by where its bone is.
void slot_passes(s32 slot);

// Says whether a point is over the ring, where there is one.
s32 over_ring(float x, float z);

// Lays the shadow of each bone of the slot under it: at the height RING over
// the ring, and at FLOOR beside it.
void slot_shadows(SceneNode *shadows, s32 slot, float ring, float floor);

// The shadows of a slot, laid and placed in the scene. In an entrance it
// also steps the change of shape of the slot's node 20.
void slot_tick(s32 slot, float ring, float floor);

#endif
