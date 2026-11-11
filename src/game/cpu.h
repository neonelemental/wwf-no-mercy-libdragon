// The control of a wrestler by the console: what it knows of the match on
// this tick, then the routine of its state.

#ifndef GAME_CPU_H
#define GAME_CPU_H

#include "model.h"

// What the console knows for one wrestler. What it reads of a wrestler
// comes with an error, which is smaller for a higher skill.
typedef struct {
    s32 state;            // which routine of D_80145A10_ovl_e plays it
    s32 phase;
    u8 unknown08[0x04];
    u16 keys[2];          // cleared before the routine of the state
    u8 unknown10[0x02];
    u8 stick[2];          // cleared too
    u8 unknown14[0x08];
    s32 plan;
    s32 skill;            // 1000 reads with no error
    s32 doing;            // what the wrestler does, by func_800FCBB0
    float meter[5];       // its Fighter.meter2C4
    s16 own[2];           // its meterAA and meterAE
    s16 own_corner;       // the quarter of the ring it stands in
    s16 own_side;         // the quarter of a turn of its place
    u8 unknown44[0x04];
    s32 foe_doing;
    s16 foe[2];
    s16 foe_extra;        // the opponent's meterB4 in some of its states, or 60
    s16 foe_corner;
    s16 foe_side;
    u8 unknown56[0x02];
    s32 held;             // ticks with bit 4 of flagsE9 and bit 1 of flags
    s16 waited;           // ticks in state 4, or in 3 with no bit of 0x28 in flags86
    u8 unknown5E[0x02];
    float seen;           // distance, with the error
    s32 heading;          // the angle about y to the opponent
    float distance;       // and how far it is, with no y
    float to_point[4];    // how far each point of D_8014D1A8_ovl_e is
    float reach;          // how far the oval of the opponent reaches, and 90
    u8 flags80;
    u8 unknown81;
    u16 flags82;          // one bit for where the two wrestlers are
    u8 unknown84[0x04];
    u8 *unknown88;
    u8 *unknown8C;
    float height;         // how far apart they are in y, with the error
    u8 unknown94[0x1C];
} Cpu;

ULTRA_SIZE(Cpu, 0xB0);
ULTRA_AT(Cpu, keys, 0x0C);
ULTRA_AT(Cpu, stick, 0x12);
ULTRA_AT(Cpu, plan, 0x1C);
ULTRA_AT(Cpu, meter, 0x28);
ULTRA_AT(Cpu, own, 0x3C);
ULTRA_AT(Cpu, foe_doing, 0x48);
ULTRA_AT(Cpu, foe_extra, 0x50);
ULTRA_AT(Cpu, held, 0x58);
ULTRA_AT(Cpu, waited, 0x5C);
ULTRA_AT(Cpu, seen, 0x60);
ULTRA_AT(Cpu, to_point, 0x6C);
ULTRA_AT(Cpu, reach, 0x7C);
ULTRA_AT(Cpu, flags80, 0x80);
ULTRA_AT(Cpu, flags82, 0x82);
ULTRA_AT(Cpu, unknown88, 0x88);
ULTRA_AT(Cpu, height, 0x90);

extern Cpu D_801596C0_ovl_e[MODEL_SLOTS];

// A value with an error of the wrestler that the console plays now: at most
// SPREAD of the value each way, less for a higher skill, and never under 0.
float cpu_noisy(float value, float spread);

// What the console reads of the two wrestlers, with the error, and the bits
// of where they are.
void cpu_senses(void);

// Plays the wrestler of a slot for a tick.
void cpu_control(s32 slot);

#endif
