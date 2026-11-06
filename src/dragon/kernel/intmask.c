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

// libdragon's boot does the rest of libultra's.
void osInitialize(void)
{
    if (osResetType == 0)
        memset(osAppNMIBuffer, 0, OS_APP_NMI_BUFSIZE);
}
