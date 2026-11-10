// libultra's threads on libdragon's kthreads. Each OSThread keeps libultra's
// size, so ultra_threads pairs it with its kthread. A kthread priority is the
// libultra priority divided by 2, as libdragon gives 0 to 127.

#include <libdragon.h>
#include <kernel.h>
#include "internal.h"

// The game's largest stack is 16 KB, and libdragon also saves the thread on
// its stack at each interrupt.
#define STACK (32 * 1024)

void __kcond_signal_isr(kcond_t *cond);

struct ultra_thread ultra_threads[ULTRA_THREADS];

// Each entry sleeps on its own condition, so a wake picks one thread.
static kcond_t wake[ULTRA_THREADS];
// osDestroyThread sets this on a thread that waits on a queue; it exits when
// it wakes.
static bool dying[ULTRA_THREADS];
static OSThread main_thread;

static int find(OSThread *t)
{
    if (!t)
        return -1;
    for (int i = 0; i < ULTRA_THREADS; i++)
        if (ultra_threads[i].os == t)
            return i;
    return -1;
}

static int bind(OSThread *t, kthread_t *k)
{
    int i = 1;
    while (i < ULTRA_THREADS && (ultra_threads[i].os || ultra_threads[i].k))
        i++;
    assertf(i < ULTRA_THREADS, "ultra_threads is full");
    ultra_threads[i].os = t;
    ultra_threads[i].k = k;
    dying[i] = false;
    kcond_init(&wake[i]);
    return i;
}

// The entry of the current kthread. The first kthread that calls in with no
// entry is the main kthread, and takes entry 0.
static int self(void)
{
    kthread_t *k = kthread_current();
    for (int i = 0; i < ULTRA_THREADS; i++)
        if (ultra_threads[i].k == k)
            return i;
    assertf(ultra_threads[0].k == NULL, "a kthread that osStartThread did not make called libultra");
    main_thread.priority = OS_PRIORITY_IDLE;
    main_thread.state = OS_STATE_RUNNING;
    ultra_threads[0].os = &main_thread;
    ultra_threads[0].k = k;
    kcond_init(&wake[0]);
    return 0;
}

// Higher priority first, and in order of arrival within a priority.
static void insert(OSThread **head, OSThread *t)
{
    OSThread **p = head;
    while (*p && (*p)->priority >= t->priority)
        p = &(*p)->next;
    t->next = *p;
    *p = t;
    t->queue = head;
}

void ultra_unlist(OSThread *t)
{
    OSThread **p = t->queue;
    if (!p)
        return;
    while (*p && *p != t)
        p = &(*p)->next;
    if (*p)
        *p = t->next;
    t->next = NULL;
    t->queue = NULL;
}

static __attribute__((noreturn)) void exit_self(void)
{
    disable_interrupts();
    int i = self();
    if (ultra_threads[i].os)
        ultra_threads[i].os->state = OS_STATE_STOPPED;
    ultra_threads[i].os = NULL;
    ultra_threads[i].k = NULL;
    enable_interrupts();
    kthread_exit(0);
}

void ultra_wait(OSThread **list)
{
    int i = self();
    OSThread *t = ultra_threads[i].os;
    insert(list, t);
    t->state = OS_STATE_WAITING;
    kcond_wait(&wake[i], NULL);
    if (dying[i])
        exit_self();
    t->state = OS_STATE_RUNNING;
}

void ultra_wake(OSThread **list, bool isr)
{
    OSThread *t = *list;
    if (!t)
        return;
    *list = t->next;
    t->next = NULL;
    t->queue = NULL;
    t->state = OS_STATE_RUNNABLE;
    int i = find(t);
    if (i < 0)
        return;
    if (isr)
        __kcond_signal_isr(&wake[i]);
    else
        kcond_signal(&wake[i]);
}

void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri)
{
    t->next = NULL;
    t->priority = pri;
    t->queue = NULL;
    t->tlnext = NULL;
    t->state = OS_STATE_STOPPED;
    t->flags = 0;
    t->id = id;
    t->fp = 0;
    t->context.pc = (u32)(uintptr_t)entry;
    t->context.a0 = (s64)(s32)(uintptr_t)arg;
    t->context.sp = (s64)(s32)((uintptr_t)sp - 16);
    t->context.sr = 0xFF03;
    t->context.rcp = 0x3F;
    t->context.fpcsr = 0x01000800;
}

// The entry of each kthread that osStartThread makes. It binds the thread
// before the game's entry runs, as a thread of higher priority than its
// creator runs before kthread_new returns.
static int trampoline(void *arg)
{
    OSThread *t = arg;
    disable_interrupts();
    if (find(t) < 0)
        bind(t, kthread_current());
    t->state = OS_STATE_RUNNING;
    enable_interrupts();
    kthread_detach(NULL);
    // libultra's FPU control word: only an invalid operation traps.
    C1_WRITE_FCR31(t->context.fpcsr);
    void (*entry)(void *) = (void (*)(void *))(uintptr_t)t->context.pc;
    entry((void *)(uintptr_t)(u32)t->context.a0);
    exit_self();
}

void osStartThread(OSThread *t)
{
    disable_interrupts();
    self();
    if (find(t) >= 0) {
        enable_interrupts();
        return;
    }
    t->state = OS_STATE_RUNNABLE;
    // With interrupts off, no other thread runs before the new thread is bound.
    // kthread_new switches to it when its priority is the same or higher.
    kthread_t *k = kthread_new("ultra", STACK, t->priority / 2, trampoline, t);
    // A thread that ran and stopped before kthread_new returned is not bound again.
    if (find(t) < 0 && t->state == OS_STATE_RUNNABLE)
        bind(t, k);
    enable_interrupts();
}

void osDestroyThread(OSThread *t)
{
    disable_interrupts();
    int me = self();
    if (t == NULL || t == ultra_threads[me].os) {
        enable_interrupts();
        exit_self();
    }
    int i = find(t);
    if (i >= 0) {
        ultra_threads[i].os = NULL;
        if (t->state == OS_STATE_WAITING && t->queue) {
            ultra_unlist(t);
            dying[i] = true;
            kcond_signal(&wake[i]);
        } else {
            // A ready thread cannot exit by itself, so it never runs again.
            kthread_suspend(ultra_threads[i].k);
            ultra_threads[i].k = NULL;
        }
    }
    t->state = OS_STATE_STOPPED;
    enable_interrupts();
}

void osYieldThread(void)
{
    kthread_yield();
}

void osSetThreadPri(OSThread *t, OSPri pri)
{
    disable_interrupts();
    if (t == NULL)
        t = ultra_threads[self()].os;
    t->priority = pri;
    if (t->state == OS_STATE_WAITING && t->queue) {
        OSThread **head = t->queue;
        ultra_unlist(t);
        insert(head, t);
    }
    int i = find(t);
    kthread_t *k = i >= 0 ? ultra_threads[i].k : NULL;
    enable_interrupts();
    if (k)
        kthread_set_pri(k, pri / 2);
}

OSPri osGetThreadPri(OSThread *t)
{
    if (t == NULL) {
        disable_interrupts();
        t = ultra_threads[self()].os;
        enable_interrupts();
    }
    return t->priority;
}

kthread_t *ultra_kthread(OSThread *t)
{
    disable_interrupts();
    int i = find(t);
    kthread_t *k = i >= 0 ? ultra_threads[i].k : NULL;
    enable_interrupts();
    return k;
}
