// The draw side of a match: which objects draw, in which order, and the
// display lists that draw them.

#ifndef GAME_DRAW_H
#define GAME_DRAW_H

#include "scene.h"

// Whether the centre of an object is in view: its depth above -150, and its
// x and y within 1.5 of the middle once divided by that depth.
inline s32 draw_in_view(SceneObject *object)
{
    float depth = object->projected[2] + 150.0f;
    if (depth <= 0.0f) {
        return 0;
    }
    float x = object->projected[0] / depth;
    if (1.5 < (double)((double)x < 0.0 ? -x : x)) {
        return 0;
    }
    float y = object->projected[1] / depth;
    if (1.5 < (double)((double)y < 0.0 ? -y : y)) {
        return 0;
    }
    return 1;
}

#endif
