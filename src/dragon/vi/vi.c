// libultra's VI calls on libdragon's vi.c. Each call keeps what the game asks
// for, and the retrace gives it to vi.c in its vblank, where libultra's VI
// manager wrote the registers. vi.c writes them, and keeps V_INTR.

#include <libdragon.h>
#include "ultra.h"

// The bits of osViSetSpecialFeatures, in pairs of on and off.
#define FEATURE_GAMMA 0x01
#define FEATURE_GAMMA_DITHER 0x04
#define FEATURE_DIVOT 0x10
#define FEATURE_DITHER_FILTER 0x40

// The bits of the control word that they set.
#define CTRL_GAMMA_DITHER 0x4
#define CTRL_GAMMA 0x8
#define CTRL_DIVOT 0x10
#define CTRL_AA_MODE 0x300
#define CTRL_DITHER_FILTER 0x10000

#define SCALE_MASK 0xFFF

// libultra's first mode of each TV type, in the game's data.
extern OSViMode D_80054C00, D_80054C50, D_80054CA0;

static bool started;
static OSViMode *mode;
static u32 ctrl;
// The Y scale of each field after osViSetYScale, until the next mode.
static bool scaled;
static u32 yscale[2];
static bool black = true, blank = true;
// libultra starts with both at the first address of RAM.
static void *next = (void *)0x80000000, *shown = (void *)0x80000000;
static u16 count = 1, left = 1;

static void set(volatile uint32_t *reg, u32 value)
{
    if (vi_read(reg) != value)
        vi_write(reg, value);
}

// In vi.c's vblank, before it writes the registers.
static void retrace(void *arg)
{
    // The crash screen masks the VI and runs the vblank itself, for its own
    // picture.
    if (!(*MI_MASK & MI_MASK_VI))
        return;

    int field = *VI_V_CURRENT & 1;
    OSViCommonRegs *com = &mode->comRegs;
    OSViFieldRegs *fld = &mode->fldRegs[field];
    set(VI_CTRL, ctrl);
    set(VI_WIDTH, com->width);
    set(VI_BURST, com->burst);
    set(VI_V_TOTAL, com->vSync);
    set(VI_H_TOTAL, com->hSync);
    set(VI_H_TOTAL_LEAP, com->leap);
    set(VI_H_VIDEO, com->hStart);
    set(VI_X_SCALE, com->xScale);
    set(VI_V_VIDEO, fld->vStart);
    set(VI_V_BURST, fld->vBurst);
    set(VI_Y_SCALE, scaled ? yscale[field] : fld->yScale);
    set(VI_ORIGIN, PhysicalAddr(next) + fld->origin);
    shown = next;
    if (black != blank) {
        blank = black;
        vi_blank(blank);
    }

    if (--left == 0) {
        ultra_event(OS_EVENT_VI);
        left = count;
    }
}

// A new mode takes the screen out of black and drops the Y scale, as
// libultra's does. vi.c moves the lines of an interlaced mode itself, on top
// of what the mode gives each field, and no mode of the game is interlaced.
void osViSetMode(OSViMode *m)
{
    assertf(!(m->comRegs.ctrl & VI_CTRL_SERRATE), "interlaced VI mode");
    disable_interrupts();
    mode = m;
    ctrl = m->comRegs.ctrl;
    scaled = false;
    black = false;
    enable_interrupts();
}

// The screen is black in libultra's first mode until the game sets its own.
void osCreateViManager(OSPri pri)
{
    if (started)
        return;
    started = true;
    osViSetMode(osTvType == 0 ? &D_80054C50 : osTvType == 2 ? &D_80054CA0 : &D_80054C00);
    black = true;
    vi_init();
    vi_install_vblank_handler(retrace, NULL);
}

void osViSetSpecialFeatures(u32 func)
{
    disable_interrupts();
    if (func & FEATURE_GAMMA)
        ctrl |= CTRL_GAMMA;
    if (func & FEATURE_GAMMA << 1)
        ctrl &= ~CTRL_GAMMA;
    if (func & FEATURE_GAMMA_DITHER)
        ctrl |= CTRL_GAMMA_DITHER;
    if (func & FEATURE_GAMMA_DITHER << 1)
        ctrl &= ~CTRL_GAMMA_DITHER;
    if (func & FEATURE_DIVOT)
        ctrl |= CTRL_DIVOT;
    if (func & FEATURE_DIVOT << 1)
        ctrl &= ~CTRL_DIVOT;
    // The filter takes the AA mode that fetches each pixel, and its end gives
    // back the AA mode of the mode.
    if (func & FEATURE_DITHER_FILTER)
        ctrl = (ctrl | CTRL_DITHER_FILTER) & ~CTRL_AA_MODE;
    if (func & FEATURE_DITHER_FILTER << 1)
        ctrl = (ctrl & ~CTRL_DITHER_FILTER) | (mode->comRegs.ctrl & CTRL_AA_MODE);
    enable_interrupts();
}

// The factor times the scale of each field, here and not at the retrace, so
// that no interrupt uses the FPU.
void osViSetYScale(f32 factor)
{
    disable_interrupts();
    for (int i = 0; i < 2; i++) {
        u32 y = mode->fldRegs[i].yScale;
        yscale[i] = (u32)(factor * (f32)(y & SCALE_MASK)) | (y & ~SCALE_MASK);
    }
    scaled = true;
    enable_interrupts();
}

void osViBlack(u8 active)
{
    black = active;
}

void osViSwapBuffer(void *buffer)
{
    next = buffer;
}

void *osViGetCurrentFramebuffer(void)
{
    return shown;
}

void *osViGetNextFramebuffer(void)
{
    return next;
}

// The retrace posts event 7 to the game once each COUNT retraces.
void osViSetEvent(OSMesgQueue *mq, OSMesg msg, u32 n)
{
    disable_interrupts();
    osSetEventMesg(OS_EVENT_VI, mq, msg);
    count = n;
    enable_interrupts();
}
