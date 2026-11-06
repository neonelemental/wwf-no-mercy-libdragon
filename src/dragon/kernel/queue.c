// libultra's message queues and events. A queue is libultra's ring of
// messages, changed with interrupts off; mtqueue and fullqueue hold the
// threads that wait to receive and to send.

#include <libdragon.h>
#include "internal.h"

static struct {
    OSMesgQueue *mq;
    OSMesg msg;
} events[OS_NUM_EVENTS];

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count)
{
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msg;
}

// Puts MSG last, or first when JAM. Interrupts are off.
static void put(OSMesgQueue *mq, OSMesg msg, bool jam)
{
    if (jam) {
        mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
        mq->msg[mq->first] = msg;
    } else {
        mq->msg[(mq->first + mq->validCount) % mq->msgCount] = msg;
    }
    mq->validCount++;
}

static s32 send(OSMesgQueue *mq, OSMesg msg, s32 flag, bool jam)
{
    disable_interrupts();
    while (mq->validCount >= mq->msgCount) {
        if (flag == OS_MESG_NOBLOCK) {
            enable_interrupts();
            return -1;
        }
        ultra_wait(&mq->fullqueue);
    }
    put(mq, msg, jam);
    ultra_wake(&mq->mtqueue, false);
    enable_interrupts();
    return 0;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag)
{
    return send(mq, msg, flag, false);
}

s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flag)
{
    return send(mq, msg, flag, true);
}

s32 ultra_send_isr(OSMesgQueue *mq, OSMesg msg)
{
    if (mq->validCount >= mq->msgCount)
        return -1;
    put(mq, msg, false);
    ultra_wake(&mq->mtqueue, true);
    return 0;
}

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag)
{
    disable_interrupts();
    while (mq->validCount == 0) {
        if (flag == OS_MESG_NOBLOCK) {
            enable_interrupts();
            return -1;
        }
        ultra_wait(&mq->mtqueue);
    }
    if (msg)
        *msg = mq->msg[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    ultra_wake(&mq->fullqueue, false);
    enable_interrupts();
    return 0;
}

void osSetEventMesg(OSEvent event, OSMesgQueue *mq, OSMesg msg)
{
    assertf(event < OS_NUM_EVENTS, "no event %lu", (unsigned long)event);
    disable_interrupts();
    events[event].mq = mq;
    events[event].msg = msg;
    enable_interrupts();
}

// From an interrupt handler. An event with no queue posts nothing, and a full
// queue drops the message, as libultra's handler does.
void ultra_event(int event)
{
    if (event < 0 || event >= OS_NUM_EVENTS || !events[event].mq)
        return;
    ultra_send_isr(events[event].mq, events[event].msg);
}

// The same post, from a thread.
void ultra_event_thread(int event)
{
    if (event < 0 || event >= OS_NUM_EVENTS)
        return;
    disable_interrupts();
    OSMesgQueue *mq = events[event].mq;
    OSMesg msg = events[event].msg;
    enable_interrupts();
    if (mq)
        osSendMesg(mq, msg, OS_MESG_NOBLOCK);
}
