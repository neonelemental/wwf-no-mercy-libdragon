#include "rope.h"

extern Rope D_80084410[ROPE_SIDES][ROPE_ROPES];
extern RopeSide D_80084370[ROPE_SIDES];
// The height of each rope of a side, and the 4 corners of a rope's section.
extern u8 D_80052888[ROPE_ROPES];
extern s8 D_80052848[4];
extern s8 D_8005284C[4];

extern Ring D_8007D780;
extern RopeNode D_800785B0[];  // the ring
extern RopeNode D_8007AC40[4];  // the thing that can break
extern RopeNode D_8007B080[2][3];
// The place of each part of the thing that can break.
extern float D_800500B8[4][3];

extern u32 D_80097EC0;
extern u32 D_800A9950;

void func_80002268(RopeNode *node);

// The game's float to u32.
static inline u32 to_u32(float x)
{
    if (2147483648.0f <= x) {
        return (u32)(s32)(x - 2147483648.0f) | 0x80000000;
    }
    return (u32)(s32)x;
}

static inline void move_to(Matrix m, float x, float y, float z)
{
    m[3][0] = x;
    m[3][1] = y;
    m[3][2] = z;
}

void rope_side(RopeVertex *vertex, int side)
{
    for (int point = 0; point < ROPE_POINTS; point++) {
        float x = (float)(point * 74 - 224);
        float out = 0;
        for (int i = 0; i < ROPE_ROPES; i++) {
            Rope *rope = &D_80084410[side][i];
            float at = rope->at;
            float part;
            if (x < at) {
                part = (x - -224.0f) / (at - -224.0f);
            } else {
                part = (224.0f - x) / (224.0f - at);
            }
            float up = rope->up * part;
            out = rope->out * part;
            up = up + (float)D_80052888[i];
            for (int corner = 0; corner < 4; corner++, vertex++) {
                vertex->y = (s32)(up + (float)D_80052848[corner]);
                vertex->z = (s32)(out + (float)D_8005284C[corner]);
            }
        }
        D_80084370[side].out[point] = to_u32(out);
    }
    RopeSide *drawn = &D_80084370[side];
    for (int i = 0; i < ROPE_PIECES; i++) {
        s16 a = drawn->out[i], b = drawn->out[i + 1];
        drawn->piece[i]->centre[2] = (float)(a < b ? b : a);
    }
}

void ringside_tick(void)
{
    Ring *ring = &D_8007D780;
    if (D_800A9950 & 0x100000) {
        return;
    }
    if (D_80097EC0 & 0x10000) {
        return;
    }
    u8 pass = 5;
    switch (ring->broken) {
    case 1:
        if (ring->broken_wait != 0) {
            ring->broken_wait = ring->broken_wait - 1;
        } else {
            u8 slow = ring->broken_slow;
            ring->broken_step = ring->broken_step + 1;
            if (slow != 0) {
                ring->broken_wait = 2;
            }
        }
        for (int i = 0; i < 4; i++) {
            float (*local)[4] = D_8007AC40[i].local;
            float *at = D_800500B8[i];
            u8 step = ring->broken_step;
            if (i == 3) {
                if (step < 7) {
                    float y = at[1] - (float)(step * 15);
                    mtx_rotate(local, 0.0f, 0.0f, 0.0f);
                    move_to(local, at[0], y, at[2]);
                }
            } else if (step >= 4) {
                if (i == 0) {
                    mtx_rotate(local, 0.0f, 0.0f, (float)((step - 3) * 10));
                } else if (i == 1) {
                    mtx_rotate(local, (float)((step - 3) * 10), 0.0f, 0.0f);
                } else {
                    mtx_rotate(local, 0.0f, 0.0f, (float)((3 - step) * 10));
                }
                move_to(local, at[0], at[1], at[2]);
            }
        }
        if (ring->broken_step == 12) {
            ring->broken = 2;
            ring->broken_step = 0;
        }
        // fallthrough
    case 0:
        pass = D_8007AC40[0].objects->key < D_800785B0[0].objects->key ? 5 : 0;
        break;
    case 2: {
        u8 step = ring->broken_step;
        if (step == 60) {
            ring->broken = 3;
            pass = 0x80;
        } else if (step >= 31 && (step & 1)) {
            pass = 0x80;
        }
        ring->broken_step = ring->broken_step + 1;
        break;
    }
    case 3:
        pass = 0x80;
        break;
    }
    for (int i = 0; i < 4; i++) {
        D_8007AC40[i].pass = pass;
    }
    func_80002268(&D_8007AC40[0]);
    for (int i = 0; i < 2; i++) {
        RopeNode *thing = D_8007B080[i];
        if (thing[0].pass & 0x80) {
            pass = 0x80;
        } else {
            pass = thing[0].objects->key < D_800785B0[0].objects->key ? 4 : 0;
        }
        for (int j = 0; j < 3; j++) {
            thing[j].pass = pass;
        }
        func_80002268(&thing[0]);
    }
}
