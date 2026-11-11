#include "cpu.h"

#include "bearing.h"
#include "oval.h"

// The wrestler that the console plays now, its opponent, its partner, the
// opponent of its partner and the partner of its opponent, and the first
// wrestler of the match. Only the control writes them.
extern Fighter *D_8016B8E8_ovl_e;
extern Fighter *D_80159618_ovl_e;
extern Fighter *D_8016B92C_ovl_e;
extern Fighter *D_8016B71C_ovl_e;
extern Fighter *D_8016B720_ovl_e;
extern Fighter *D_8016B8EC_ovl_e;
// What the console knows for that wrestler.
extern Cpu *D_8015A6F0_ovl_e;

// The middle of the wrestler and of its opponent.
extern float D_80159674_ovl_e[3];
extern float D_8016B9C0_ovl_e[3];
// Ten bytes of the wrestler's profile, from 0x19 on.
extern u8 D_80159624_ovl_e, D_80159510_ovl_e, D_80162A78_ovl_e, D_8015A6F4_ovl_e, D_80159980_ovl_e;
extern u8 D_8016B590_ovl_e, D_8016B9E1_ovl_e, D_8016B93C_ovl_e, D_8016B9E0_ovl_e;
extern s8 D_8016B9CE_ovl_e;

extern u8 D_800AA050[];
extern u8 D_8016B350_ovl_e[MODEL_SLOTS][0x88];
extern float D_8014D1A8_ovl_e[4][3];
extern u8 D_80145298_ovl_e[], D_801452AC_ovl_e[];
// The state that each slot was last in, and for how many ticks.
extern s16 D_801452C4_ovl_e[MODEL_SLOTS];
extern s16 D_801452CC_ovl_e[MODEL_SLOTS];
// The routine of each state.
extern void (*D_80145A10_ovl_e[])(void *);

extern u32 D_80097EC0;
extern u32 D_800A9950;

// Routines of the game that are still assembly.
s32 func_8012DFF4(void);                                    // a random number
s32 func_800FCBB0(Fighter *fighter, Fighter *me, s32 was);  // what a wrestler does
s32 func_800FD128(Fighter *fighter);                        // the quarter of the ring it stands in
s32 func_800F0E78(s32 slot, s32 state, s32 last, s32 from);
void func_800FD040(u8 *choices);
void func_800FCEB4(s32 state);                              // starts a state
void func_800FCF74(s32 state);                              // starts a state that is not the one it is in
void func_800F0D04(s32 slot);
void func_800F0DA4(s32 slot);
void ultra_call(void (*entry)(void *), void *arg);

// A word of flags in the profile of a wrestler.
static u32 profile_flags(Fighter *fighter)
{
    u8 *p = fighter->profile + 0x34;
    return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

float cpu_noisy(float value, float spread)
{
    float wide = value * (float)(1.0 - (double)D_8015A6F0_ovl_e->skill * 0.001);
    float narrow = value * spread;
    if (narrow < wide) {
        wide = narrow;
    }
    s32 random = func_8012DFF4();
    float seen = (float)((double)value + (double)wide * 0.001 * (double)random - (double)wide * 0.5);
    if ((double)seen < 0.0) {
        seen = 0.0f;
    }
    return seen;
}

// The bit of where the two wrestlers are, by bits 0, 1, 2 and 10 of their flags.
static s32 where(u16 own, u16 other)
{
    if (own & 1) {
        return other & 1 ? 1 : other & 0x400 ? 0x2000 : other & 2 ? 2 : 4;
    }
    if (own & 2) {
        return other & 1 ? 8 : other & 2 ? 0x10 : 0x20;
    }
    if (own & 4) {
        return other & 1 ? 0x40 : other & 2 ? 0x80 : 0x100;
    }
    if (own & 0x400) {
        return other & 1 ? 0x4000 : other & 0x400 ? 0x8000 : 0x100;
    }
    return 0x100;
}

void cpu_senses(void)
{
    Fighter *me = D_8016B8E8_ovl_e;
    Fighter *foe = D_80159618_ovl_e;
    Cpu *cpu = D_8015A6F0_ovl_e;

    cpu->own[0] = (s32)cpu_noisy((float)me->meterAA, 0.1f);
    cpu->foe[0] = (s32)cpu_noisy((float)foe->meterAA, 0.3f);
    cpu->own[1] = (s32)cpu_noisy((float)me->meterAE, 0.1f);
    cpu->foe[1] = (s32)cpu_noisy((float)foe->meterAE, 0.2f);
    cpu->seen = cpu_noisy(cpu->distance, 0.2f);
    cpu->height = cpu_noisy(cpu->height, 0.1f);
    for (int i = 0; i < 5; i++) {
        cpu->meter[i] = cpu_noisy(me->meter2C4[i], 0.1f);
    }
    switch (foe->state) {
    case 0x18:
    case 0x31:
    case 0x32:
    case 0x34:
        cpu->foe_extra = (s32)cpu_noisy((float)foe->meterB4, 0.3f);
        break;
    case 0x2E:
        cpu->foe_extra = 60;
        break;
    default:
        cpu->foe_extra = 0;
        break;
    }

    cpu->flags82 = cpu->flags82 & 0x1E00;
    s32 bit = where(me->flags, foe->flags);
    cpu->flags80 = cpu->flags80 & 0xEF;
    cpu->flags82 = cpu->flags82 | bit;
    s32 both = 0;
    if (profile_flags(me) & 0x2000) {
        both = (me->flagsEE << 2) & 0x10;
        if (profile_flags(foe) & 0x2000) {
            both = 0x10;
        }
    }
    cpu->flags80 = cpu->flags80 | both;
    cpu->flags80 = cpu->flags80 & 0xDF;
    if ((D_80097EC0 & 0x2000) && !(D_800A9950 & 0x40) && !(me->flagsE9 & 0x10)) {
        cpu->flags80 = cpu->flags80 | 0x20;
    }
    cpu->flags80 = cpu->flags80 & 0xBF;
    if ((D_80097EC0 & 0x4000) && (D_800A9950 & 0x400)) {
        cpu->flags80 = cpu->flags80 | 0x40;
    }
}

void cpu_control(s32 slot_word)
{
    s8 slot = slot_word;
    Fighter *first = D_80159990_ovl_e;
    Fighter *me = &first[slot];
    Fighter *foe = &first[me->opponent];
    Cpu *cpu = &D_801596C0_ovl_e[slot];
    Oval oval;

    D_8016B8E8_ovl_e = me;
    D_8016B92C_ovl_e = &first[me->partner];
    D_8016B8EC_ovl_e = first;
    D_8015A6F0_ovl_e = cpu;
    D_80159618_ovl_e = foe;
    D_8016B71C_ovl_e = &first[first[me->partner].opponent];
    D_8016B720_ovl_e = &first[foe->partner];
    cpu->stick[0] = 0;
    cpu->keys[0] = 0;
    cpu->keys[1] = 0;
    cpu->stick[1] = 0;
    cpu->unknown88 = D_800AA050 + 0x40 + 0x4D10 * me->slot;
    cpu->unknown8C = D_8016B350_ovl_e[me->slot];

    cpu->distance = distance_to(me->point[1], foe->point[1]);
    cpu->heading = heading_to(me->point[1], foe->point[1]);
    cpu->height = flat_absf(me->point[1][1] - foe->point[1][1]);
    cpu_senses();
    cpu->doing = func_800FCBB0(me, me, cpu->doing);
    cpu->foe_doing = func_800FCBB0(foe, me, cpu->foe_doing);
    for (int i = 0; i < 4; i++) {
        cpu->to_point[i] = distance_to(me->point[1], D_8014D1A8_ovl_e[i]);
    }
    fighter_oval(foe, &oval);
    cpu->reach = (float)((double)oval_reach(&oval, me->point[1]) + 90.0);
    cpu->own_corner = func_800FD128(me);
    cpu->foe_corner = func_800FD128(foe);
    cpu->own_side = way_quarter(me->point[1]);
    cpu->foe_side = way_quarter(foe->point[1]);
    D_80159674_ovl_e[0] = me->point[1][0];
    D_80159674_ovl_e[1] = me->point[1][1];
    D_80159674_ovl_e[2] = me->point[1][2];
    D_8016B9C0_ovl_e[0] = foe->point[1][0];
    D_8016B9C0_ovl_e[1] = foe->point[1][1];
    D_8016B9C0_ovl_e[2] = foe->point[1][2];
    u8 *profile = me->profile;
    D_80159624_ovl_e = profile[0x19];
    D_80159510_ovl_e = profile[0x1A];
    D_80162A78_ovl_e = profile[0x1E];
    D_8015A6F4_ovl_e = profile[0x1F];
    D_80159980_ovl_e = profile[0x22];
    D_8016B590_ovl_e = profile[0x23];
    D_8016B9E1_ovl_e = profile[0x24];
    D_8016B93C_ovl_e = profile[0x25];
    D_8016B9E0_ovl_e = profile[0x26];
    me->flags88 = me->flags88 & 0xFFFE;
    D_8016B9CE_ovl_e = profile[0x27];

    if (foe->state == 0x2E) {
        if (!(me->flagsE9 & 1) && (foe->flags80 & 8)) {
            cpu->foe_doing = 15;
        }
    } else if (foe->state == 0x1B || foe->state == 0x1C) {
        if (cpu->plan != 6 && me->opponent != me->partner && (cpu->flags82 & 0x10B)
            && cpu->own[0] - cpu->foe[0] >= -99 && cpu->own[0] >= 51 && cpu->foe[0] >= 31 && cpu->foe[1] < 90
            && func_800F0E78(foe->slot, 0x1B, 16, 0) < 3 && func_800F0E78(foe->slot, 0x1C, 16, 0) < 3) {
            func_800FD040(D_8016B9CE_ovl_e == 2 ? D_80145298_ovl_e : D_801452AC_ovl_e);
        }
    }

    if (me->state == 4 || (me->state == 3 && !(me->flags86 & 0x28))) {
        cpu->waited = cpu->waited + 1;
        if (cpu->waited >= 151) {
            func_800FCEB4(0);
        }
    } else {
        cpu->waited = 0;
    }

    switch (cpu->doing) {
    case 3:
        func_800FCEB4(0x1E);
        break;
    case 6:
        func_800FCEB4(0x1D);
        break;
    case 7:
    case 15:
    case 20:
    case 38:
    case 40:
    case 59:
    case 60:
    case 61:
        func_800FCF74(0x1F);
        break;
    case 13: {
        s32 state = cpu->state;
        if (state == 0x1A || state == 0x66 || state == 0x67 || state == 0x6F || state == 0x70
            || state == 0x75 || state == 0x78) {
            break;
        }
        func_800FCF74(0x19);
        break;
    }
    case 16:
    case 17:
    case 18:
        func_800FCF74(0x35);
        break;
    case 24:
        if (!(D_80097EC0 & 0x100000)) {
            func_800FCF74(0x36);
        }
        break;
    case 25:
        if (cpu->state != 0x41 && cpu->state != 0x42) {
            func_800FCEB4(0x41);
            cpu->phase = 4;
        }
        break;
    case 28:
        if (cpu->state != 0x75) {
            func_800FCF74(15);
        }
        break;
    case 34:
        func_800FCF74(0x45);
        break;
    case 35:
        func_800FCF74(0x46);
        break;
    }
    if (cpu->foe_doing == 0x29) {
        func_800FCF74(0x73);
    }
    if (me->state == 0xA5 && (cpu->flags82 & 1) && foe->state == 0x31 && (foe->flags84 & 2)) {
        func_800FCF74(0x74);
    }
    if ((me->flagsE9 & 0x10) && (me->flags & 2)) {
        cpu->held = cpu->held + 1;
        if (cpu->held >= 151) {
            cpu->held = 0;
            func_800FCEB4(0);
        }
    } else {
        cpu->held = 0;
    }

    func_800F0D04(me->slot);
    // 300 ticks in one state start state 0.
    s16 *last = &D_801452C4_ovl_e[me->slot];
    s16 *ticks = &D_801452CC_ovl_e[me->slot];
    if (*last == cpu->state) {
        *ticks = *ticks + 1;
        if (*ticks >= 300) {
            func_800FCF74(0);
            *last = cpu->state;
            *ticks = 0;
        }
    } else {
        *last = cpu->state;
        *ticks = 0;
    }
    func_800F0DA4(me->slot);
    ultra_call(D_80145A10_ovl_e[cpu->state], NULL);
}
