// libultra's time and timers on libdragon's timer. Time is in Count ticks, as
// libultra gives it. get_ticks extends Count only if it runs each 91 s or
// less, so the VI handler calls it each VI.

#include <libdragon.h>
#include "internal.h"

#define TIMERS 16

extern bool __isr_force_schedule;

// OSTimer keeps libultra's size, so each libdragon timer is in this table.
static struct timer {
    OSTimer *t;
    timer_link_t link;
} timers[TIMERS];

OSTime osGetTime(void)
{
    return get_ticks();
}

u32 osGetCount(void)
{
    return TICKS_READ();
}

// In the timer interrupt. After the countdown, the period is the interval.
static void fire(int ovfl, void *ctx)
{
    struct timer *s = ctx;
    OSTimer *t = s->t;
    if (t->interval)
        s->link.set = (u32)t->interval;
    else
        s->t = NULL;
    if (t->mq)
        ultra_send_isr(t->mq, t->msg);
}

int osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg)
{
    t->next = NULL;
    t->prev = NULL;
    t->interval = interval;
    t->value = countdown ? countdown : interval;
    t->mq = mq;
    t->msg = msg;
    assertf(t->value <= INT32_MAX && interval <= UINT32_MAX, "timer too long");

    disable_interrupts();
    struct timer *s = NULL;
    for (int i = 0; i < TIMERS && !s; i++)
        if (timers[i].t == t)
            s = &timers[i];
    if (s)
        stop_timer(&s->link);
    for (int i = 0; i < TIMERS && !s; i++)
        if (!timers[i].t)
            s = &timers[i];
    assertf(s, "no free timer");
    s->t = t;
    start_timer_context(&s->link, (int)t->value, interval ? TF_CONTINUOUS : TF_ONE_SHOT, fire, s);
    // At this libdragon, start_timer_context links only a timer that is
    // disabled. A stop and a restart link it once in either case.
    stop_timer(&s->link);
    restart_timer(&s->link);
    // A timer that is due at once fires here, out of an interrupt. Its wake of
    // a thread of higher priority then waits for the next interrupt, so yield.
    bool wake = !exception_is_running() && __isr_force_schedule;
    if (wake)
        __isr_force_schedule = false;
    enable_interrupts();
    if (wake)
        kthread_yield();
    return 0;
}
