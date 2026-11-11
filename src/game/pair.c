#include "pair.h"

#include "bearing.h"

// A thing in the ring that a wrestler can touch.
typedef struct {
    u8 unknown00[0x04];
    s32 kind;
    u8 unknown08[0x18];
    u16 flags;
    u8 unknown22[0x22];
    float place[3];
    u8 unknown50[0x64];
} Thing;

ULTRA_SIZE(Thing, 0xB4);
ULTRA_AT(Thing, kind, 0x04);
ULTRA_AT(Thing, flags, 0x20);
ULTRA_AT(Thing, place, 0x44);

#define THINGS 7
extern Thing D_8016AE10_ovl_e[THINGS];

// Routines of the game that are still assembly.
void func_8012EAC4(s32 angle, float length, float *out);  // a way of a length, at an angle about y
void func_8012EDCC(float *way);                            // brings a way to the length 1
void func_8012EE4C(float *way, float by, float *out);     // a way times a number
void func_80131BD0(Fighter *fighter, float *by);          // moves a wrestler and its points
s32 func_80130170(Fighter *fighter);

// A round oval at the middle of a wrestler.
static void round_oval(Oval *oval, Fighter *fighter)
{
    oval->half[1] = 0.0f;
    oval->angle = fighter->angle[1];
    oval->half[0] = 33.0f;
    oval->half[2] = 33.0f;
    oval->centre[0] = fighter->point[1][0];
    oval->centre[1] = fighter->point[1][1];
    oval->centre[2] = fighter->point[1][2];
}

static void measure(Near *near, Near *own, s32 who)
{
    Bearing b;
    bearing_between(own->oval.centre, near->oval.centre, &b);
    near->who = who;
    near->distance = b.length[1];
    near->angle = (s32)b.angle[1];
}

void pair_list(Fighter *fighter, Near *list)
{
    Near *own = &list[NEAR_MAX];
    for (int i = NEAR_MAX; i >= 0; i--) {
        list[i].who = -1;
    }
    if ((fighter->flags80 & 8) || (fighter->flags8A & 0x2000) || (fighter->flags & 0x400)) {
        fighter_oval(fighter, &own->oval);
    } else {
        round_oval(&own->oval, fighter);
    }

    s32 count = 0;
    for (s32 i = 0; i < MODEL_SLOTS; i++) {
        Fighter *other = &D_80159990_ovl_e[i];
        if (other->id == -1 || other->slot == fighter->slot) {
            continue;
        }
        if (other->state == 0xA3 && other->state_time < 15) {
            continue;
        }
        u16 flags = other->flags;
        if (!(fighter->flags & flags & 0x7FF) && !((fighter->flags | flags) & 0x1000)) {
            continue;
        }
        u16 flags8A = other->flags8A;
        if (flags8A & 0x40) {
            continue;
        }
        if ((flags & 0x400) && other->wall != fighter->wall_by) {
            continue;
        }
        Near *near = &list[count];
        if ((other->flags80 & 8) || (flags8A & 0x2000) || (flags & 0x400)) {
            fighter_oval(other, &near->oval);
        } else {
            round_oval(&near->oval, other);
        }
        measure(near, own, i);
        count++;
    }

    if (!(fighter->flags & 2)) {
        for (s32 i = 0; i < THINGS; i++) {
            Thing *thing = &D_8016AE10_ovl_e[i];
            if (thing->kind != 0x19 || !(thing->flags & 4) || !(thing->flags & 0x100)) {
                continue;
            }
            float y = (float)((double)thing->place[1] - 126.0);
            if (fighter->flags & 0x1000) {
                y = fighter->place[1];
            }
            if (!((double)flat_absf(y - fighter->place[1]) < 150.0)) {
                continue;
            }
            Near *near = &list[count];
            near->oval.half[0] = 60.0f;
            near->oval.half[2] = 60.0f;
            near->oval.half[1] = 0.0f;
            near->oval.angle = 0;
            near->oval.centre[0] = thing->place[0];
            near->oval.centre[1] = thing->place[1];
            near->oval.centre[2] = thing->place[2];
            measure(near, own, i + 100);
            count++;
        }
    }

    for (s32 i = 0; i < count - 1; i++) {
        for (s32 j = i + 1; j < count; j++) {
            if (list[j].distance < list[i].distance) {
                Near other = list[i];
                list[i] = list[j];
                list[j] = other;
            }
        }
    }
}

s32 pair_push(Fighter *fighter, Fighter *second, float by, s32 axes, s32 skip, s32 mode)
{
    Near list[NEAR_MAX + 1];
    Near *own = &list[NEAR_MAX];
    float push[3] = { 0.0f, 0.0f, 0.0f };
    float way[3];
    Fighter *other = NULL;
    s32 first = -1;

    if (!(axes & 0xFFFF)) {
        axes = 0xF00;
    }
    pair_list(fighter, list);
    for (Near *near = list; near < own; near++) {
        if (near->who == -1 || near->who == skip) {
            continue;
        }
        float reach = oval_reach(&own->oval, near->oval.centre);
        reach = reach + oval_reach(&near->oval, own->oval.centre);
        if (near->who < MODEL_SLOTS) {
            other = &D_80159990_ovl_e[near->who];
            if (other->flags80 & 8) {
                reach = (float)((double)reach + 20.0);
            } else if (((other->flags8A & 0x2000) || (other->flags & 0x400))
                && !(other->flags8A & 0x4000) && func_80130170(other)) {
                reach = (float)((double)reach + 20.0);
            }
        }
        float over = -1.0f;
        if (near->distance <= reach) {
            over = reach - near->distance;
        }
        if ((double)over == -1.0) {
            continue;
        }
        if (mode != 0) {
            if (fighter->flags & 0x400) {
                // On a wall, the push across the wall goes along it, up or down.
                func_8012EAC4(turn_sum(near->angle, 0x800), over, way);
                float up;
                if (fighter->wall & 1) {
                    up = way[0];
                    way[0] = 0.0f;
                } else {
                    up = way[2];
                    way[2] = 0.0f;
                }
                way[1] = up;
                way[0] = way[0] / by;
                way[2] = way[2] / by;
                way[1] = way[1] / by;
                if (fighter->state == 0x6E) {
                    way[1] = 0.0f;
                }
                func_80131BD0(fighter, way);
                if (second) {
                    func_80131BD0(second, way);
                }
            } else {
                s32 off = turn_between(fighter->angle[1], near->angle);
                s32 angle = (off < 0 ? (s32)-(u32)off : off) < 0x200 ? fighter->angle[1] : near->angle;
                if (mode == 2) {
                    if (near->who < MODEL_SLOTS && (other->flagsE9 & 0x10)) {
                        angle = near->angle;
                    } else {
                        angle = turn_sum(near->angle, -0x200);
                    }
                }
                func_8012EAC4(turn_sum(angle, 0x800), over, way);
                way[0] = axes & 0x300 ? way[0] / by : 0.0f;
                way[2] = axes & 0xC00 ? way[2] / by : 0.0f;
                way[1] = 0.0f;
                push[0] = push[0] + way[0];
                push[1] = push[1] + way[1];
                push[2] = push[2] + way[2];
            }
        }
        if (first == -1) {
            first = near->who;
        }
        if (near->who >= 100) {
            fighter->touching = near->who;
        }
    }
    if (first == -1 || mode == 0 || (fighter->flags & 0x400)) {
        return first;
    }
    // The sum of the pushes, at most 7.5 long once it is 15 or more.
    if (15.0 <= (double)flat_length(push[0], push[2])) {
        func_8012EDCC(push);
        func_8012EE4C(push, 7.5f, push);
    }
    func_80131BD0(fighter, push);
    if (second) {
        func_80131BD0(second, push);
    }
    return first;
}
