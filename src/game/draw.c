#include "draw.h"

extern inline s32 draw_in_view(SceneObject *object);

// The first object of each node that draws, and their count.
extern SceneObject *D_80060BA0[900];
extern s16 D_800619B2;
// The pass of each of them this frame: 0xFF once it has its place in the order.
extern u8 D_800619B8[900];
// The chains in the order that they draw in, and their count.
extern SceneObject *D_80061D40[900];
extern s16 D_800619B4;

void draw_scan(s32 turn, s32 pass, s32 kind)
{
    turn = (s16)turn;
    pass = (u8)pass;
    kind = (s8)kind;
    for (u32 i = 0; i < (u32)D_800619B2; i++) {
        if (D_800619B8[i] != pass) {
            continue;
        }
        SceneObject *object = D_80060BA0[i];
        if (object->kind != kind) {
            continue;
        }
        D_800619B8[i] = 0xFF;
        D_80061D40[D_800619B4] = object;
        if (kind == 1) {
            if (!draw_in_view(object)) {
                continue;
            }
            for (; object; object = object->next) {
                guMtxF2L(object->node->product, &object->mtx[turn]);
            }
        } else {
            for (; object; object = object->next) {
                if (object->node->list) {
                    object->list = object->node->list;
                }
                guMtxF2L(object->node->product, &object->mtx[turn]);
            }
        }
        D_800619B4++;
    }
}

// The pass that a chain of kind 1 draws in, by the pass of its node.
extern u8 D_80043618[];

void draw_order(s32 turn)
{
    turn = (s16)turn;
    D_800619B4 = 0;
    for (u32 i = 0; i < (u32)D_800619B2; i++) {
        SceneObject *object = D_80060BA0[i];
        u8 pass = object->node->pass;
        if (pass & 0x80) {
            D_800619B8[i] = 0xFF;
            guMtxF2L(object->node->product, &object->mtx[turn]);
        } else if (object->kind == 1) {
            D_800619B8[i] = D_80043618[pass];
        } else {
            D_800619B8[i] = pass;
        }
    }
    draw_scan(turn, 0, 0);
    draw_scan(turn, 0, 1);
    draw_scan(turn, 6, 0);
    draw_scan(turn, 3, 0);
    draw_scan(turn, 4, 1);
    draw_scan(turn, 4, 0);
    draw_scan(turn, 5, 1);
    draw_scan(turn, 5, 0);
}
