// libultra's events from libdragon's interrupts. libdragon's handler has
// acknowledged each interrupt before these run. joybus owns the SI interrupt,
// so src/dragon/si posts the SI's event.

#include <libdragon.h>
#include <kernel.h>
#include "internal.h"

// As libultra's handler does: event 4 for a task that is done or has yielded,
// and event 11 for any other break.
static void sp(void)
{
    u32 status = *SP_STATUS;
    *SP_STATUS = SP_WSTATUS_CLEAR_INTR | SP_WSTATUS_CLEAR_SIG3;
    ultra_event(status & (SP_STATUS_SIG1 | SP_STATUS_SIG2) ? OS_EVENT_SP : OS_EVENT_SP_BREAK);
}

static void ai(void) { ultra_event(OS_EVENT_AI); }
static void dp(void) { ultra_event(OS_EVENT_DP); }
static void reset(void) { ultra_event(OS_EVENT_PRENMI); }

// get_ticks keeps the time only when it runs often enough (time.c). The
// retrace of src/dragon/vi posts event 7.
static void vi(void)
{
    get_ticks();
}

void ultra_start(void)
{
    timer_init();
    kernel_init();
    register_SP_handler(sp);
    register_AI_handler(ai);
    register_VI_handler(vi);
    register_DP_handler(dp);
    register_RESET_handler(reset);
    osSetIntMask(OS_IM_ALL);
}
