// libultra's interrupt mask and boot call. libdragon owns the status register,
// so the mask keeps only its RCP bits, which go to MI_MASK.

#include <libdragon.h>
#include <string.h>
#include "internal.h"

static OSIntMask mask = OS_IM_ALL;

OSIntMask osSetIntMask(OSIntMask im)
{
    disable_interrupts();
    OSIntMask old = mask;
    mask = im;
    // Bit B of the RCP bits is line B of the MI: SP, SI, AI, VI, PI, DP.
    // MI_MASK takes a clear bit and a set bit for each line.
    u32 rcp = (im >> 16) & 0x3F, w = 0;
    for (int b = 0; b < 6; b++)
        w |= (rcp & (1u << b)) ? 2u << (2 * b) : 1u << (2 * b);
    *MI_MASK = w;
    enable_interrupts();
    return old;
}

// libdragon counts the depth of disable_interrupts, so each pair must balance.
u32 __osDisableInt(void)
{
    u32 old = get_interrupts_state() == INTERRUPTS_ENABLED;
    disable_interrupts();
    return old;
}

void __osRestoreInt(u32 old)
{
    enable_interrupts();
}

// The words of the game that libultra's boot call fills. The split names each
// by its address in the retail image.
extern s32 D_800A9980;          // set once the boot call ran
extern u8 D_800A5FB4[5];        // the cartridge's PI handle, from its type
extern u8 D_8009ED2C[5];        // the disk drive's PI handle, from its type
extern u64 D_80054AF0;          // osClockRate
extern s32 D_80054AF8;          // osViClock

// The type, latency, page size, release and pulse of a PI handle, from the
// four timing registers that start at REGS.
static void pi_timing(u8 *handle, volatile u32 *regs)
{
    handle[0] = 7;
    handle[1] = regs[0];
    handle[4] = regs[1];
    handle[2] = regs[2];
    handle[3] = regs[3];
}

// libultra's boot call. libdragon's boot has set the status register, the
// exception vectors and the FPU; no code here uses the TLB or the watch
// register, which libultra also set.
void osInitialize(void)
{
    D_800A9980 = 1;

    // The PIF stops the console unless it is told that the boot is done.
    // joybus takes each SI interrupt as its own, so the write leaves none.
    volatile u32 *si_status = (u32 *)0xA4800018, *pif = (u32 *)0xBFC007FC;
    disable_interrupts();
    while (*si_status & 3) {}
    u32 w = *pif;
    while (*si_status & 3) {}
    *pif = w | 8;
    while (*si_status & 3) {}
    *si_status = 0;
    enable_interrupts();

    pi_timing(D_800A5FB4, (u32 *)0xA4600014);
    pi_timing(D_8009ED2C, (u32 *)0xA4600024);
    D_80054AF0 = D_80054AF0 * 3 / 4;
    if (osResetType == 0)
        memset(osAppNMIBuffer, 0, OS_APP_NMI_BUFSIZE);
    D_80054AF8 = osTvType == 0 ? 0x2F5B2D2 : osTvType == 2 ? 0x2E6025C : 0x2E6D354;

    // The AI: control, DAC rate and bit rate.
    volatile u32 *ai = (u32 *)0xA4500000;
    ai[2] = 1;
    ai[4] = 0x3FFF;
    ai[5] = 0xF;
}
