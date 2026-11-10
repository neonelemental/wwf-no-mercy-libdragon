// The model of a wrestler: its pose from the animation data, and the matrix
// of each joint.

#ifndef GAME_MODEL_H
#define GAME_MODEL_H

#include "ultra.h"

// Reads the pose of one joint: a word of three angles, then its place when
// the word says so. Gives the byte after the pose.
u8 *pose_read(u8 *data, float angle[3], float place[3]);

#endif
