#include "rope.h"

#include "model.h"

extern Rope D_80084410[ROPE_SIDES][ROPE_ROPES];
extern RopeSide D_80084370[ROPE_SIDES];
extern SceneNode D_80084000[ROPE_SIDES];
// The height of each rope of a side, and the 4 corners of a rope's section.
extern u8 D_80052888[ROPE_ROPES];
extern s8 D_80052848[4];
extern s8 D_8005284C[4];

extern Ring D_8007D780;
// The ring, and with it the 10 things that are sorted against it.
extern SceneNode D_800785B0[13];
extern SceneNode D_8007A290;
// The cage of a cage match: its 7 parts that shake, and the wall that fades
// in, with that wall's model.
extern SceneNode D_8007A3B0[7];
extern SceneNode D_8007AB28;
extern u32 D_8007AC04;
extern u8 D_8007AC08[];
extern SceneNode D_8007AC40[4];  // the thing that can break
extern SceneNode D_8007B080[2][3];
// The place of each part of the thing that can break.
extern float D_800500B8[4][3];
// The wall as it was last written: how solid, and how far it shook.
extern u8 D_8005089C;
extern s8 D_8005089D;

extern s16 D_800A8968;  // the place of the match
extern u8 game_mode;
extern u32 D_80097EC0;
extern u32 D_800A9950;
extern u16 D_8009FDE0;
extern s16 D_800A9352;

void func_8000E670(float *x, float *y, float *z);
void func_8001AF80(u8 *model, u32 solid, s32 shift);
void func_8001D21C(void);
void func_8001DA04(void);
s32 func_8001EB04(void);
void func_8001ED20(void);
void func_8001FFA8(void);
void func_80022178(void);
float func_80034B40(float x);
s32 func_800EE640(void);
s32 func_800EE69C(void);

// The game's absolute value, which keeps a zero's sign.
static inline float absf(float x)
{
    return (double)x < 0.0 ? -x : x;
}

static inline void move_to(Matrix m, float x, float y, float z)
{
    m[3][0] = x;
    m[3][1] = y;
    m[3][2] = z;
}

void rope_side(Vertex *vertex, int side)
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
        D_80084370[side].out[point] = float_to_u32(out);
    }
    RopeSide *drawn = &D_80084370[side];
    for (int i = 0; i < ROPE_PIECES; i++) {
        s16 a = drawn->out[i], b = drawn->out[i + 1];
        drawn->piece[i]->centre[2] = (float)(a < b ? b : a);
    }
}

void rope_swing(void)
{
    func_80022178();
    for (int side = 0; side < ROPE_SIDES; side++) {
        for (int i = 0; i < ROPE_ROPES; i++) {
            Rope *rope = &D_80084410[side][i];
            float step = rope->up_step;
            rope->ticks--;
            if (step != 0.0f) {
                rope->up = rope->up + step;
                if (rope->ticks <= 0) {
                    rope->up_end = (float)((double)-rope->up_end * 0.6);
                    if ((double)absf(rope->up_end) < 0.1) {
                        float other = rope->out_step;
                        rope->up = 0;
                        rope->up_end = 0;
                        rope->up_step = 0;
                        if (other == 0.0f) {
                            rope->state = 0;
                        }
                    } else {
                        rope->up_step = (float)((double)(rope->up_end - rope->up) * (1.0 / 6.0));
                    }
                }
            }
            step = rope->out_step;
            if (step != 0.0f) {
                rope->out = rope->out + step;
                if (rope->ticks > 0) {
                    continue;
                }
                rope->out_end = (float)((double)-rope->out_end * (rope->state != 7 ? 0.6 : 0.7));
                if ((double)absf(rope->out_end) < 0.1) {
                    float other = rope->up_step;
                    rope->out = 0;
                    rope->out_end = 0;
                    rope->out_step = 0;
                    if (other == 0.0f) {
                        rope->state = 0;
                    }
                } else {
                    rope->out_step = (float)((double)(rope->out_end - rope->out) * (rope->state != 7 ? 1.0 / 6.0 : 1.0 / 13.0));
                }
            }
            if (rope->ticks > 0) {
                continue;
            }
            if (rope->state != 7) {
                rope->ticks = 6;
            } else {
                float end = rope->out_end;
                rope->ticks = 13;
                if (0.0f < end) {
                    rope->state = 6;
                }
            }
        }
        rope_side(D_80084000[side].vertices, side);
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
    scene_walk(&D_8007AC40[0]);
    for (int i = 0; i < 2; i++) {
        SceneNode *thing = D_8007B080[i];
        if (thing[0].pass & 0x80) {
            pass = 0x80;
        } else {
            pass = thing[0].objects->key < D_800785B0[0].objects->key ? 4 : 0;
        }
        for (int j = 0; j < 3; j++) {
            thing[j].pass = pass;
        }
        scene_walk(&thing[0]);
    }
}

void ring_tick(void)
{
    Ring *ring = &D_8007D780;
    u32 solid = 0xFF;
    if (0.1 < (double)absf(ring->bounce_end - ring->bounce)) {
        ring->bounce = ring->bounce + ring->bounce_step;
    } else {
        ring->bounce_end = (float)-((double)ring->bounce_end * 0.3);
        if ((double)absf(ring->bounce_end) < 0.1) {
            ring->bounce_end = 0;
            ring->bounce = 0;
            ring->bounce_step = 0;
        } else {
            ring->bounce_step = (float)((double)(ring->bounce_end - ring->bounce) * (1.0 / 6.0));
        }
    }
    if (ring->place != 30 && !(D_800785B0[3].pass & 0x80)) {
        // Each of the 10 things draws before the ring or after it.
        for (int i = 3; i < 13; i++) {
            SceneNode *node = &D_800785B0[i];
            float ring_z = D_800785B0[0].objects->projected[2];
            SceneObject *object = node->objects;
            if ((double)object->projected[2] < (double)ring_z - 400.0) {
                node->pass = 5;
                object->bias = 500.0f;
                node->objects->kind = 0;
            } else {
                node->pass = 0;
                object->bias = 25000.0f;
                node->objects->kind = 1;
            }
        }
        if ((D_80097EC0 & 0x100000) && game_mode == 3) {
            D_8007A290.pass |= 0x80;
        }
    }
    move_to(D_800785B0[0].local, 0.0f, ring->bounce, 0.0f);
    scene_walk(&D_800785B0[0]);
    if (D_8007AC04 != 0) {
        // A wall of the cage shows from far away, or in a match as the
        // wrestlers climb, and gets more solid by 8 a tick.
        float x, y, z;
        func_8000E670(&x, &y, &z);
        float far = func_80034B40(x * x + z * z);
        if (game_mode == 1) {
            s32 climb = func_800EE69C();
            if (climb == 2) {
                ring->wall_solid = 1;
            }
            if (ring->wall_solid != 0 || climb != 0) {
                float top = 0.0f;
                for (int i = 0; i < 4; i++) {
                    float height = D_800A1130[i][1].world[1];
                    if (top < height) {
                        top = height;
                    }
                }
                far = (top - 100.0f) * (1.0f / 300.0f);
                if ((double)far < 0.0) {
                    far = 0;
                } else if (1.0 < (double)far) {
                    far = 1.0f;
                }
                if (ring->wall_solid == 0) {
                    solid = float_to_u32(far * 255.0f);
                }
            } else {
                far = 0.0f;
            }
        } else if (far < 600.0f) {
            far = 0.0f;
        }
        s32 shift = 0;
        D_800A9352 = 0;
        if (ring->shake != 0) {
            shift = ((ring->shake & 2) << 1) - 2;
            for (int i = 0; i < 7; i++) {
                move_to(D_8007A3B0[i].local, 0.0f, (float)(s8)shift, 0.0f);
            }
            ring->shake = ring->shake - 1;
            s32 climbers = func_800EE640();
            D_8009FDE0 = climbers;
            if (climbers & 1) {
                D_8009FDE0 = 1;
            } else if (climbers & 2) {
                D_8009FDE0 = 2;
            }
            if (D_8009FDE0 != 0) {
                D_800A9352 = (s8)shift;
            }
        }
        if (far == 0.0f) {
            D_8005089C = 0;
            D_8007AB28.pass |= 0x80;
        } else {
            u8 last = D_8005089C;
            if ((solid & 0xFF) != last || (s8)shift != D_8005089D) {
                D_8007AB28.pass &= 0x7F;
                if (game_mode != 3 && last + 8 < (s32)(solid & 0xFF)) {
                    solid = last + 8;
                }
                func_8001AF80(D_8007AC08, solid & 0xFF, D_8005089D);
                D_8005089C = solid;
                D_8005089D = shift;
            }
        }
    }
    if (ring->place != 30) {
        if (D_80097EC0 & 0x10000) {
            scene_walk(&D_8007A3B0[0]);
            scene_walk(&D_8007AB28);
        }
        if (D_80097EC0 & 0x100000) {
            scene_walk(&D_8007A290);
        }
        func_8001D21C();
        func_8001DA04();
        ringside_tick();
        if (game_mode != 3) {
            func_8001ED20();
        }
    }
    ring->ticks++;
}

void arena_tick(void)
{
    s16 arena = D_800A8968;
    if (arena >= 10 && arena != 30) {
        if (game_mode != 3 || (s16)func_8001EB04() == 0) {
            func_8001FFA8();
            return;
        }
    }
    rope_swing();
    ring_tick();
}
