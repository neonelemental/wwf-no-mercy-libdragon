// Starts the kernel with 8 threads at half the game's priorities, and main as
// the idle thread. The VI handler posts a count to a ring and wakes a thread
// that takes each count. Two threads check sums of doubles across switches.

#include <libdragon.h>
#include <kernel.h>
#include <kqueue.h>
#include <string.h>

#define VI_TARGET 10000
#define RING 64
#define STACK (16 * 1024)

void __kcond_signal_isr(kcond_t *cond);

// Some emulators do not do MI repeat mode, so the display and the console fill
// with the CPU. The link sends each call of sys_hw_memset* here.
void *__wrap_sys_hw_memset(void *ptr, uint8_t value, size_t len)
{
    return memset(ptr, value, len);
}

void *__wrap_sys_hw_memset16(void *ptr, uint16_t value, size_t len)
{
    for (size_t i = 0; i < len / 2; i++)
        ((uint16_t *)ptr)[i] = value;
    return ptr;
}

void *__wrap_sys_hw_memset32(void *ptr, uint32_t value, size_t len)
{
    for (size_t i = 0; i < len / 4; i++)
        ((uint32_t *)ptr)[i] = value;
    return ptr;
}

static volatile uint32_t ring[RING];
static volatile uint32_t ring_head, ring_tail;
static volatile uint32_t vi_sent, vi_taken, vi_bad, vi_full;
static kcond_t vi_cond;

// The same counts through a kqueue, to see if kqueue_try_put_isr drops one.
static kqueue_t *vi_queue;
static volatile uint32_t q_sent, q_full, q_taken;

static volatile uint8_t ran[9];
static volatile uint32_t ticks[8];
static volatile double vi_mean;

struct sums { double step, want[4]; volatile uint32_t rounds, bad; };
static struct sums sums_a = { 0.25, { 125125.0, 250250.0, 375375.0, 500500.0 } };
static struct sums sums_b = { 1.5, { 750750.0, 1501500.0, 2252250.0, 3003000.0 } };

static void vi_handler(void)
{
    if (vi_sent == VI_TARGET)
        return;
    vi_sent++;
    if (ring_head - ring_tail == RING) {
        vi_full++;
    } else {
        ring[ring_head % RING] = vi_sent;
        ring_head++;
    }
    __kcond_signal_isr(&vi_cond);
    if (kqueue_try_put_isr(vi_queue, (void *)(uintptr_t)vi_sent))
        q_sent++;
    else
        q_full++;
}

static int vi_thread(void *arg)
{
    double mean = 0;
    ran[0] = 1;
    for (;;) {
        disable_interrupts();
        while (ring_tail == ring_head)
            kcond_wait(&vi_cond, NULL);
        uint32_t v = ring[ring_tail % RING];
        ring_tail++;
        enable_interrupts();
        if (v != vi_taken + 1)
            vi_bad++;
        vi_taken = v;
        // The woken thread also uses the FPU, so a lost FPU save shows in the sums.
        mean = mean * 0.5 + (double)v * 0.5;
        vi_mean = mean;
    }
    return 0;
}

static int queue_thread(void *arg)
{
    ran[1] = 1;
    for (;;) {
        kqueue_get(vi_queue);
        q_taken++;
    }
    return 0;
}

static int sleep_thread(void *arg)
{
    int n = (int)arg;
    ran[n] = 1;
    for (;;) {
        kthread_sleep(TICKS_FROM_MS(n));
        ticks[n]++;
    }
    return 0;
}

// The doubles and the print wait for the VI, so that they do not depend on the
// timer interrupt.
static void wait_vi(void)
{
    kirq_wait_t w = kirq_begin_wait_vi();
    kirq_wait(&w);
}

static int sums_thread(void *arg)
{
    struct sums *s = arg;
    ran[s == &sums_a ? 5 : 6] = 1;
    for (;;) {
        double t0 = 0, t1 = 0, t2 = 0, t3 = 0;
        for (int k = 1; k <= 1000; k++) {
            double x = k * s->step;
            t0 += x;
            t1 += x * 2;
            t2 += x * 3;
            t3 += x * 4;
        }
        if (t0 != s->want[0] || t1 != s->want[1] || t2 != s->want[2] || t3 != s->want[3])
            s->bad++;
        s->rounds++;
        wait_vi();
    }
    return 0;
}

static int print_thread(void *arg)
{
    ran[7] = 1;
    for (;;) {
        int threads = 0;
        for (int i = 0; i < 9; i++)
            threads += ran[i];
        bool sums_ok = sums_a.bad == 0 && sums_b.bad == 0 && sums_a.rounds && sums_b.rounds;
        console_clear();
        printf("libdragon o32, stock IPL3\n");
        printf("threads %d\n", threads);
        printf("%u of %u\n", (unsigned)vi_taken, (unsigned)vi_sent);
        printf("order bad %u, ring full %u\n", (unsigned)vi_bad, (unsigned)vi_full);
        printf("queue %u of %u, full %u\n", (unsigned)q_taken, (unsigned)q_sent, (unsigned)q_full);
        printf("doubles %s %u %u\n", sums_ok ? "ok" : "bad", (unsigned)sums_a.rounds, (unsigned)sums_b.rounds);
        printf("sleeps %u %u %u\n", (unsigned)ticks[2], (unsigned)ticks[3], (unsigned)ticks[4]);
        if (vi_sent == VI_TARGET && vi_taken == VI_TARGET)
            printf("done\n");
        console_render();
        wait_vi();
    }
    return 0;
}

int main(void)
{
    timer_init();
    kernel_init();
    kcond_init(&vi_cond);
    vi_queue = kqueue_new(8);
    console_init();
    console_set_render_mode(RENDER_MANUAL);

    // The game's priorities halved: VI 254, PI 150, scheduler 120, audio start
    // 110, graphics start 100, audio 80, graphics 70, game 8. Main is the idle thread.
    kthread_new("vi", STACK, 127, vi_thread, NULL);
    kthread_new("pi", STACK, 75, sleep_thread, (void *)2);
    kthread_new("sched", STACK, 60, queue_thread, NULL);
    kthread_new("audiostart", STACK, 55, sleep_thread, (void *)3);
    kthread_new("gfxstart", STACK, 50, sleep_thread, (void *)4);
    kthread_new("audio", STACK, 40, sums_thread, &sums_a);
    kthread_new("gfx", STACK, 35, sums_thread, &sums_b);
    kthread_new("game", STACK, 4, print_thread, NULL);
    register_VI_handler(vi_handler);

    for (;;)
        ran[8] = 1;
}
