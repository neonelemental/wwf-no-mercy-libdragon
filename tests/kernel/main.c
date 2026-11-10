// Runs libultra's thread, queue and event calls on libdragon's kernel, with
// the game's threads and queues: 8 created threads at the game's priorities,
// and main as the idle thread. It prints a count for each path.

#include <libdragon.h>
#include "ultra.h"

#define VI_TARGET 3600
#define JAMS 9
#define VI_MSG ((OSMesg)0x29A)
#define SI_MSG ((OSMesg)0x5)
#define SENT ((OSMesg)0x100)
#define JAM(n) ((OSMesg)(uintptr_t)(0x200 + (n)))
// A timer of 60 VIs.
#define TIMER_60_VI ((OSTime)TICKS_PER_SECOND * 1001 / 1000)

enum { T_PI, T_VI, T_SCHED, T_AUDIOSTART, T_GFXSTART, T_AUDIO, T_GFX, T_GAME, CREATED };
static OSThread threads[CREATED], extra, due;
static volatile int own[CREATED];

// The created queues of the game, at their sizes.
static OSMesgQueue dmaq, timerq, viq, schedq, taskq, gfxq, audioq, siq, jamdoneq, blockq, dueq;
static OSMesg dma_buf[48], timer_buf[1], vi_buf[5], sched_buf[8], task_buf[8];
static OSMesg gfx_buf[16], audio_buf[4], si_buf[1], jamdone_buf[1], block_buf[1], due_buf[1];
static OSMesg fan_msg = (OSMesg)0x80057188;
static OSTimer timer, due_timer;

static volatile u32 vi_sent, vi_taken, sched_bad, sched_full;
static volatile u32 fan[3], posts, post_taken, fired;
static volatile int jam_first = -1, jam_tail_ok;
static volatile int stage, due_ran;
static u32 fails;

// The words of the game that osInitialize fills.
s32 D_800A9980, D_80054AF8;
u8 D_800A5FB4[5], D_8009ED2C[5];
u64 D_80054AF0 = 62500000;

static void check(bool ok, int bit)
{
    if (!ok)
        fails |= 1u << bit;
}

static void mark(int id)
{
    own[id] = ultra_kthread(&threads[id]) == kthread_current();
}

static void vi_handler(void)
{
    get_ticks();
    if (vi_sent < VI_TARGET) {
        vi_sent++;
        ultra_event(OS_EVENT_VI);
    }
}

static void vi_thread(void *arg)
{
    mark(T_VI);
    for (;;) {
        OSMesg m;
        osRecvMesg(&viq, &m, OS_MESG_BLOCK);
        if (m == VI_MSG)
            vi_taken++;
        if (osSendMesg(&schedq, VI_MSG, OS_MESG_NOBLOCK) < 0)
            sched_full++;
    }
}

// Thread 4 of the game: one message a VI to three queues.
static void sched_thread(void *arg)
{
    mark(T_SCHED);
    for (;;) {
        OSMesg m;
        osRecvMesg(&schedq, &m, OS_MESG_BLOCK);
        if (m != VI_MSG)
            sched_bad++;
        osSendMesg(&taskq, fan_msg, OS_MESG_BLOCK);
        osSendMesg(&gfxq, fan_msg, OS_MESG_BLOCK);
        osSendMesg(&audioq, fan_msg, OS_MESG_BLOCK);
    }
}

// Posts event 5 from a thread after each message.
static void gfxstart_thread(void *arg)
{
    mark(T_GFXSTART);
    for (;;) {
        OSMesg m;
        osRecvMesg(&taskq, &m, OS_MESG_BLOCK);
        if (m == fan_msg)
            fan[0]++;
        posts++;
        ultra_event_thread(OS_EVENT_SI);
    }
}

// Counts each post of event 5.
static void audiostart_thread(void *arg)
{
    mark(T_AUDIOSTART);
    for (;;) {
        OSMesg m;
        osRecvMesg(&siq, &m, OS_MESG_BLOCK);
        if (m == SI_MSG)
            post_taken++;
    }
}

static void fan_thread(void *arg)
{
    int n = (int)(uintptr_t)arg;
    OSMesgQueue *q = n == 1 ? &gfxq : &audioq;
    mark(n == 1 ? T_GFX : T_AUDIO);
    for (;;) {
        OSMesg m;
        osRecvMesg(q, &m, OS_MESG_BLOCK);
        if (m == fan_msg)
            fan[n]++;
    }
}

// One message, then a jam at each firing of the timer. Its entry returns.
static void pi_thread(void *arg)
{
    mark(T_PI);
    osSendMesg(&dmaq, SENT, OS_MESG_NOBLOCK);
    for (int n = 1; n <= JAMS; n++) {
        OSMesg m;
        osSetTimer(&timer, TIMER_60_VI, 0, &timerq, (OSMesg)(uintptr_t)n);
        osRecvMesg(&timerq, &m, OS_MESG_BLOCK);
        if (m == (OSMesg)(uintptr_t)n)
            fired++;
        osJamMesg(&dmaq, JAM(n), OS_MESG_NOBLOCK);
    }
    osSendMesg(&jamdoneq, NULL, OS_MESG_NOBLOCK);
}

// At the creator's priority, for osYieldThread and osDestroyThread.
static void extra_thread(void *arg)
{
    stage = 1;
    osYieldThread();
    stage = 2;
    osRecvMesg(&blockq, NULL, OS_MESG_BLOCK);
    stage = 3;
}

// Above the creator, for a timer that is due when it is set.
static void due_thread(void *arg)
{
    osRecvMesg(&dueq, NULL, OS_MESG_BLOCK);
    due_ran = 1;
}

static void wait_vi(void)
{
    kirq_wait_t w = kirq_begin_wait_vi();
    kirq_wait(&w);
}

static void calls_once(void)
{
    osSetThreadPri(NULL, 10);
    check(osGetThreadPri(NULL) == 10, 0);
    osSetThreadPri(NULL, 8);
    check(osGetThreadPri(&threads[T_GAME]) == 8, 1);
    osSetThreadPri(&threads[T_GFX], 72);
    check(osGetThreadPri(&threads[T_GFX]) == 72, 2);
    osSetThreadPri(&threads[T_GFX], 70);

    osCreateThread(&extra, 9, extra_thread, NULL, NULL, 8);
    osStartThread(&extra);
    for (int i = 0; i < 4 && stage != 2; i++)
        osYieldThread();
    check(stage == 2, 4);
    check(blockq.mtqueue == &extra, 5);
    osDestroyThread(&extra);
    check(ultra_kthread(&extra) == NULL && extra.state == OS_STATE_STOPPED, 6);
    check(blockq.mtqueue == NULL, 7);

    osCreateThread(&due, 10, due_thread, NULL, NULL, 12);
    osStartThread(&due);
    osSetTimer(&due_timer, 1, 0, &dueq, NULL);
    check(due_ran, 3);

    u32 c0 = osGetCount();
    OSTime t0 = osGetTime();
    for (volatile int i = 0; i < 1000; i++)
        ;
    check((s32)(osGetCount() - c0) > 0 && osGetTime() > t0, 8);

    u32 vi0 = vi_sent;
    check(osSetIntMask(OS_IM_NONE) == OS_IM_ALL, 9);
    check(osSetIntMask(OS_IM_VI) == OS_IM_NONE, 10);
    u32 s = __osDisableInt();
    check(s == 1 && get_interrupts_state() == INTERRUPTS_DISABLED, 11);
    __osRestoreInt(s);
    check(get_interrupts_state() == INTERRUPTS_ENABLED, 12);
    wait_vi();
    wait_vi();
    check(vi_sent > vi0, 13);
}

// After the ninth jam: 9 jams, newest first, then the sent message.
static void read_jams(void)
{
    OSMesg m;
    int n = 0;
    while (n < JAMS && osRecvMesg(&dmaq, &m, OS_MESG_NOBLOCK) == 0 && m == JAM(JAMS - n))
        n++;
    jam_first = n;
    jam_tail_ok = n == JAMS && osRecvMesg(&dmaq, &m, OS_MESG_NOBLOCK) == 0 && m == SENT &&
                  osRecvMesg(&dmaq, &m, OS_MESG_NOBLOCK) < 0;
    check(ultra_kthread(&threads[T_PI]) == NULL && threads[T_PI].state == OS_STATE_STOPPED, 14);
}

static void game_thread(void *arg)
{
    mark(T_GAME);
    calls_once();
    for (u32 frame = 0;; frame++) {
        if (jam_first < 0 && osRecvMesg(&jamdoneq, NULL, OS_MESG_NOBLOCK) == 0)
            read_jams();
        if (frame % 30 == 0 || vi_sent == VI_TARGET) {
            int owned = 0;
            for (int i = 0; i < CREATED; i++)
                owned += own[i];
            bool done = jam_first >= 0;
            console_clear();
            printf("libultra calls on libdragon\n");
            printf("threads %d of %d\n", owned, CREATED);
            printf("vi %u of %u\n", (unsigned)vi_taken, VI_TARGET);
            printf("fan %u of %u\n", (unsigned)(fan[0] + fan[1] + fan[2]), 3 * VI_TARGET);
            printf("post %u of %u\n", (unsigned)post_taken, VI_TARGET);
            printf("lost sched %u, bad %u, posts %u\n", (unsigned)sched_full, (unsigned)sched_bad, (unsigned)posts);
            if (done)
                printf("jam first %d%s\n", jam_first, jam_tail_ok ? "" : ", tail bad");
            printf("timer %u\n", (unsigned)fired);
            if (fails || owned != CREATED)
                printf("calls bad %lx\n", (unsigned long)fails);
            else if (done)
                printf("calls ok\n");
            if (vi_sent == VI_TARGET && vi_taken == VI_TARGET)
                printf("done\n");
            console_render();
        }
        wait_vi();
    }
}

int main(void)
{
    timer_init();
    kernel_init();
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    osAppNMIBuffer[0] = 0xAA;
    osInitialize();
    check(osAppNMIBuffer[0] == 0, 15);

    osCreateMesgQueue(&dmaq, dma_buf, 48);
    osCreateMesgQueue(&timerq, timer_buf, 1);
    osCreateMesgQueue(&viq, vi_buf, 5);
    osCreateMesgQueue(&schedq, sched_buf, 8);
    osCreateMesgQueue(&taskq, task_buf, 8);
    osCreateMesgQueue(&gfxq, gfx_buf, 16);
    osCreateMesgQueue(&audioq, audio_buf, 4);
    osCreateMesgQueue(&siq, si_buf, 1);
    osCreateMesgQueue(&jamdoneq, jamdone_buf, 1);
    osCreateMesgQueue(&blockq, block_buf, 1);
    osCreateMesgQueue(&dueq, due_buf, 1);
    osSetEventMesg(OS_EVENT_VI, &viq, VI_MSG);
    osSetEventMesg(OS_EVENT_SI, &siq, SI_MSG);

    // The game's ids and priorities.
    osCreateThread(&threads[T_PI], 0, pi_thread, NULL, NULL, 150);
    osCreateThread(&threads[T_VI], 0, vi_thread, NULL, NULL, 254);
    osCreateThread(&threads[T_SCHED], 4, sched_thread, NULL, NULL, 120);
    osCreateThread(&threads[T_AUDIOSTART], 18, audiostart_thread, NULL, NULL, 110);
    osCreateThread(&threads[T_GFXSTART], 17, gfxstart_thread, NULL, NULL, 100);
    osCreateThread(&threads[T_AUDIO], 3, fan_thread, (void *)2, NULL, 80);
    osCreateThread(&threads[T_GFX], 3, fan_thread, (void *)1, NULL, 70);
    osCreateThread(&threads[T_GAME], 6, game_thread, NULL, NULL, 8);
    register_VI_handler(vi_handler);
    for (int i = 0; i < CREATED; i++)
        osStartThread(&threads[i]);

    osSetThreadPri(NULL, OS_PRIORITY_IDLE);
    for (;;)
        ;
}
