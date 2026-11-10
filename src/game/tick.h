// One tick of a match: the passes of the engine over the scene, in their
// order, and the frame that goes to the graphics thread.

#ifndef GAME_TICK_H
#define GAME_TICK_H

#include "ultra.h"

// Runs one tick of the match. With HELD not 0, the arena does not tick.
void match_tick(s32 held);

#endif
