#include "draw.h"

extern inline s32 draw_in_view(SceneObject *object);

// The pass of each chain of D_80060BA0 this frame: 0xFF once it has its place
// in the order.
extern u8 D_800619B8[900];
// The chains in the order that they draw in, and their count.
extern SceneObject *D_80061D40[900];
extern s16 D_800619B4;

// The pass that a chain of kind 1 draws in, by the pass of its node.
extern u8 D_80043618[];

// The passes of the draw order: each takes the chains of one pass and one kind.
static const struct {
    u8 pass, kind;
} PASSES[8] = {{0, 0}, {0, 1}, {6, 0}, {3, 0}, {4, 1}, {4, 0}, {5, 1}, {5, 0}};

#define NONE 0xFFFF

void draw_order(s32 turn)
{
    turn = (s16)turn;
    // Which of the passes takes a chain, by its kind and its pass.
    s8 taker[2][8];
    for (u32 i = 0; i < 16; i++) {
        taker[i >> 3][i & 7] = -1;
    }
    for (u32 i = 0; i < 8; i++) {
        taker[PASSES[i].kind][PASSES[i].pass] = i;
    }
    // The chains of each pass, in the order of the table: the first, and for
    // each chain the next. The scan goes from the last chain to the first.
    u16 first[8], next[900];
    for (u32 i = 0; i < 8; i++) {
        first[i] = NONE;
    }
    for (u32 i = (u32)D_800619B2; i-- > 0;) {
        SceneObject *object = D_80060BA0[i];
        u32 pass = object->node->pass;
        if (pass & 0x80) {
            D_800619B8[i] = 0xFF;
            guMtxF2L(object->node->product, &object->mtx[turn]);
            continue;
        }
        u32 kind = object->kind;
        if (kind == 1) {
            pass = D_80043618[pass];
        }
        s32 by = kind < 2 && pass < 8 ? taker[kind][pass] : -1;
        if (by < 0) {
            D_800619B8[i] = pass;
            continue;
        }
        D_800619B8[i] = 0xFF;
        next[i] = first[by];
        first[by] = i;
    }
    // A chain of kind 1 that is out of view leaves its place to the next chain.
    u32 placed = 0;
    for (u32 by = 0; by < 8; by++) {
        for (u32 i = first[by]; i != NONE; i = next[i]) {
            SceneObject *object = D_80060BA0[i];
            D_80061D40[placed] = object;
            if (PASSES[by].kind == 1) {
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
            placed++;
        }
    }
    D_800619B4 = placed;
}

// The two buffers that the tasks go to in turn, and whose turn it is.
extern DrawBuffer *D_8005D8E0[2];
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
    Gfx *g = D_8005D8E0[D_80043610]->list;
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

// The code that starts a task on the RSP, and where it ends.
extern u64 D_80040380[];
extern u64 D_80040450[];
// The microcode of a task with its data: of kind 0, and of any other kind.
extern u64 D_800410B0[];
extern u64 D_80054FD0[];
extern u64 D_80042450[];
extern u64 D_800553F0[];
// The stack of the microcode, and where it keeps a task that yields.
extern u64 D_8005D8F0[0x400 / 8];
extern u64 D_8005DCF0[0xC00 / 8];

void func_800005E0(SchedTask *task);  // sends a task to the scheduler

void draw_task_end(DrawFrame *frame, Gfx *list, s32 last, s32 kind)
{
    void *image = frame->image;
    DrawBuffer *buffer = D_8005D8E0[D_80043610];
    list[0].w0 = 0xE9000000;
    list[0].w1 = 0;
    list[1].w0 = 0xDF000000;
    list[1].w1 = 0;
    list += 2;
    SchedTask *sched = &buffer->sched;
    OSTask_t *task = &sched->task.t;
    task->type = 1;
    task->flags = 2;
    task->ucode_boot = D_80040380;
    task->ucode_boot_size = (u32)D_80040450 - (u32)D_80040380;
    if ((s16)kind == 0) {
        task->ucode = D_800410B0;
        task->ucode_data = D_80054FD0;
    } else {
        task->ucode = D_80042450;
        task->ucode_data = D_800553F0;
    }
    task->ucode_size = 0x1000;
    task->ucode_data_size = 0x800;
    task->dram_stack = D_8005D8F0;
    task->dram_stack_size = sizeof D_8005D8F0;
    task->output_buff = NULL;
    task->output_buff_size = NULL;
    task->data_ptr = (u64 *)D_8005D8E0[D_80043610];
    task->data_size = (s16)(list - D_8005D8E0[D_80043610]->list) * sizeof(Gfx);
    task->yield_data_ptr = D_8005DCF0;
    task->yield_data_size = sizeof D_8005DCF0;
    sched->next = NULL;
    sched->flags = (s16)last ? 0x63 : 0x03;
    sched->image = image;
    // The address of the pointer, not of the buffer: the size alone makes
    // this a write back of the whole data cache.
    osWritebackDCache(&D_8005D8E0[D_80043610], sizeof(DrawBuffer));
    func_800005E0(sched);
    D_80043610 = (s16)(D_80043610 + 1) % 2;
}

s32 draw_task(DrawFrame *frame, s32 turn, s32 first)
{
    first = (s16)first;
    Gfx *list = draw_list_start(frame, first == 0);
    s32 kind = D_80061D40[first]->kind;
    s32 next = (s16)draw_list_objects(&list, (s16)turn, first);
    draw_task_end(frame, list, next >= D_800619B4, (s16)kind);
    return next;
}

// The queue that the main thread wakes the graphics thread by, the frame
// that it gives, or null for none, and the queue of the answer.
extern OSMesgQueue D_80060B00;
extern DrawFrame *D_80060B3C;
extern OSMesgQueue D_80060B20;
// What the scheduler tells the graphics thread: 1 for a retrace, 2 for a
// task that is done, 4 for the reset button.
extern OSMesgQueue D_80060AA8;
// The two pictures that the frames draw into in turn.
extern u8 D_8038F800[];
extern u8 D_803C7C00[];
// Set once the reset button is down.
extern s16 D_80043612;

void func_8000071C(s32 which);
void func_800036DC(s32 on);
void osViSetYScale(f32 factor);
void osViBlack(u8 active);

// Takes the next message of the scheduler.
static void told(u32 *retraces, u32 *pending)
{
    OSMesg what = NULL;
    osRecvMesg(&D_80060AA8, &what, OS_MESG_BLOCK);
    switch (*(s16 *)what) {
    case 1:
        ++*retraces;
        break;
    case 2:
        --*pending;
        break;
    case 4:
        D_80043612 = 1;
        *pending += 0x40;
        osViSetYScale(1.0f);
        osViBlack(1);
        func_800036DC(0);
        break;
    }
}

void draw_thread(void *arg)
{
    u32 image = 0;
    u32 retraces = 2;
    u32 pending = 0;
    s32 turn = 0;
    for (;;) {
        osRecvMesg(&D_80060B00, NULL, OS_MESG_BLOCK);
        DrawFrame *frame = D_80060B3C;
        if (frame) {
            func_8000071C(0);
            frame->image = image & 1 ? D_803C7C00 : D_8038F800;
            image ^= 1;
            draw_order(turn);
        }
        osSendMesg(&D_80060B20, NULL, OS_MESG_BLOCK);
        while (retraces < 2) {
            told(&retraces, &pending);
        }
        if (!frame) {
            continue;
        }
        osWritebackDCacheAll();
        retraces = 0;
        s32 next = 0;
        do {
            while (next < D_800619B4 && pending < 2) {
                next = draw_task(frame, turn, next);
                pending++;
            }
            told(&retraces, &pending);
        } while (next < D_800619B4 || pending);
        turn ^= 1;
    }
}
