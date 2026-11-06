// The wait and the wake that the thread, queue and timer calls share. Each
// runs with interrupts off.

#ifndef KERNEL_INTERNAL_H
#define KERNEL_INTERNAL_H

#include <stdbool.h>
#include "ultra.h"

// Puts the current thread on LIST, in priority order, and sleeps until a
// wake takes it from there.
void ultra_wait(OSThread **list);

// Takes the first thread from LIST and wakes it. ISR is true in an interrupt.
void ultra_wake(OSThread **list, bool isr);

// Takes T from the list that it waits on.
void ultra_unlist(OSThread *t);

// A send with no block, from an interrupt.
s32 ultra_send_isr(OSMesgQueue *mq, OSMesg msg);

#endif
