// The draw side of a match: which objects draw, in which order, and the
// display lists that draw them.

#ifndef GAME_DRAW_H
#define GAME_DRAW_H

#include "scene.h"

// A command of a display list.
typedef struct {
    u32 w0, w1;
} Gfx;

// What the main thread gives the graphics thread to draw a frame by.
typedef struct {
    void *image;  // the picture to draw into, which the graphics thread picks
    u16 persp;    // the scale of the RSP's perspective divide
    u16 unknown6;
} DrawFrame;

// What the scheduler takes for a task.
typedef struct SchedTask {
    struct SchedTask *next;
    u32 state;
    u32 flags;    // 0x03: the task needs the RSP and the RDP; 0x60 more for the last task of a frame
    void *image;  // the picture that the task draws into
    OSTask task;
    u8 unknown50[0x28];
} SchedTask;

// The buffer of a task: its display list, then the task itself.
typedef struct {
    Gfx list[1500];
    SchedTask sched;
} DrawBuffer;

ULTRA_SIZE(Gfx, 8);
ULTRA_SIZE(DrawFrame, 8);
ULTRA_AT(DrawFrame, persp, 0x04);
ULTRA_SIZE(SchedTask, 0x78);
ULTRA_AT(SchedTask, flags, 0x08);
ULTRA_AT(SchedTask, image, 0x0C);
ULTRA_AT(SchedTask, task, 0x10);
ULTRA_SIZE(DrawBuffer, 0x2F58);
ULTRA_AT(DrawBuffer, sched, 0x2EE0);

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

// Makes the draw order of a frame: the chains of each of eight passes in
// turn, each with the fixed-point matrices of its objects for this turn. A
// chain of kind 1 whose head is out of view gets no place. An object whose
// node keeps its own product gets its matrix too.
void draw_order(s32 turn);
// Starts the display list of a task in the buffer of this turn, and gives
// where its next command goes. With clear, the list first fills the picture.
Gfx *draw_list_start(DrawFrame *frame, s32 clear);
// Writes the commands of the chains from first on, as far as they are of one
// kind: the matrix and the vertices of each object, then the lists. Gives
// the place of the first chain that it left.
s32 draw_list_objects(Gfx **list, s32 turn, s32 first);
// Ends the display list of a task and sends the task to the scheduler. With
// last, the task is the last of its frame, and kind picks its microcode.
void draw_task_end(DrawFrame *frame, Gfx *list, s32 last, s32 kind);
// Writes the display list of one task and sends the task. Gives the place
// of the first chain that it left.
s32 draw_task(DrawFrame *frame, s32 turn, s32 first);
// The graphics thread: for each frame of the main thread it makes the draw
// order, holds the frame until two retraces after the last one, and sends
// its tasks, at most two at a time.
void draw_thread(void *arg);

#endif
