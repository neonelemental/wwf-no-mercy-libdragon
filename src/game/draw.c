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

// The two buffers that the display lists go to in turn, and whose turn it is.
extern Gfx *D_8005D8E0[2];
extern s16 D_80043610;
// What segment 1 of a display list is.
extern void *D_800BD94C;

u32 osVirtualToPhysical(void *address);

// libultra's osVirtualToPhysical, which for an address of KSEG0 or KSEG1
// is a mask.
static inline u32 physical(void *address)
{
    if ((u32)address - 0x80000000 < 0x40000000) {
        return (u32)address & 0x1FFFFFFF;
    }
    return osVirtualToPhysical(address);
}

Gfx *draw_list_start(DrawFrame *frame, s32 clear)
{
    Gfx *g = D_8005D8E0[D_80043610];
    g[0].w0 = 0xDB060000;
    g[0].w1 = 0;
    g[1].w0 = 0xDB060004;
    g[1].w1 = physical(D_800BD94C);
    g[2].w0 = 0xDB0E0000;
    g[2].w1 = frame->persp;
    g[3].w0 = 0xDE000000;
    g[3].w1 = 0x01000010;
    g += 4;
    if ((s16)clear) {
        g[0].w0 = 0xFF1001DF;
        g[0].w1 = (u32)frame->image - 0x80000000;
        g[1].w0 = 0xE3000A01;
        g[1].w1 = 0x00300000;
        g[2].w0 = 0xE200001C;
        g[2].w1 = 0;
        g[3].w0 = 0xF7000000;
        g[3].w1 = 0x00010001;
        g += 4;
        // 80 strips of 3 lines, each 480 wide.
        for (u32 i = 0; i < 80; i++, g++) {
            g->w0 = 0xF677C000 | ((3 * (i + 1) - 1) & 0x3FF) << 2;
            g->w1 = ((3 * i) & 0x3FF) << 2;
        }
        g->w0 = 0xE7000000;
        g->w1 = 0;
        g++;
    }
    g[0].w0 = 0xD7000002;
    g[0].w1 = 0xFFFFFFFF;
    g[1].w0 = 0xD9C0F9FA;
    g[1].w1 = 0;
    g[2].w0 = 0xD9FFFFFF;
    g[2].w1 = 0x00200404;
    g[3].w0 = 0xE3000A01;
    g[3].w1 = 0;
    g[4].w0 = 0xE3001001;
    g[4].w1 = 0x8000;
    return g + 5;
}

// The PRIM and the ENV colour of a node's lists: 4 of each for each turn.
extern u8 D_80062B68[2][4][4];
extern u8 D_80062B88[2][4][4];

static u32 colour(u8 table[2][4][4], u32 which)
{
    u8 *c = &table[0][0][0] + which + (D_80043610 << 4);
    return c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
}

s32 draw_list_objects(Gfx **list, s32 turn, s32 first)
{
    turn = (s16)turn;
    u32 i = (s16)first;
    SceneObject **chain = &D_80061D40[(s16)first];
    s32 kind = (*chain)->kind;
    for (; i < (u32)D_800619B4; i++, chain++) {
        SceneObject *object = *chain;
        if (object->kind != kind) {
            break;
        }
        for (; object; object = object->next) {
            Gfx *g = (*list)++;
            g->w0 = 0xDC38000E;
            g->w1 = (u32)&object->mtx[turn] - 0x80000000;
            g = (*list)++;
            g->w0 = 0xDB0C0000;
            g->w1 = 0x00010000;
            if (!object->vertices) {
                continue;
            }
            // At most 32 vertices to a command.
            u32 at = physical(object->vertices);
            u32 count = object->count;
            u32 place = object->first;
            while (count > 32) {
                g = (*list)++;
                place += 32;
                count -= 32;
                g->w1 = at;
                g->w0 = 0x01020000 | (place & 0x7F) << 1;
                at += 32 * sizeof(Vertex);
            }
            g = (*list)++;
            g->w0 = 0x01000000 | (count & 0xFF) << 12 | ((place + count) & 0x7F) << 1;
            g->w1 = at;
        }
        for (object = *chain; object; object = object->next) {
            if (!object->list) {
                continue;
            }
            SceneNode *node = object->node;
            if (!node->raw) {
                u32 which = (node->colour & 0xF) << 2;
                Gfx *g = (*list)++;
                g->w0 = 0xFA000000;
                g->w1 = colour(D_80062B68, which);
                g = (*list)++;
                g->w0 = 0xFB000000;
                g->w1 = colour(D_80062B88, which);
            }
            Gfx *g = (*list)++;
            g->w0 = 0xDE000000;
            g->w1 = (u32)object->list - 0x80000000;
        }
    }
    return (s16)i;
}

// Ends the display list of a task and sends the task. With last, it is the
// last of its frame, and kind picks its microcode.
void func_80002DE4(DrawFrame *frame, Gfx *list, s32 last, s32 kind);

s32 draw_task(DrawFrame *frame, s32 turn, s32 first)
{
    first = (s16)first;
    Gfx *list = draw_list_start(frame, first == 0);
    s32 kind = D_80061D40[first]->kind;
    s32 next = (s16)draw_list_objects(&list, (s16)turn, first);
    func_80002DE4(frame, list, next >= D_800619B4, (s16)kind);
    return next;
}
