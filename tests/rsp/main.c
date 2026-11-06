// Runs the game's graphics microcode as a raw RSP task beside rspq and rdpq,
// five times a frame, and times each switch with the Count register.
// Way 1 closes rdpq and rspq and starts them again. Way 2 keeps them, and saves
// IMEM, DMEM and the RSP and RDP registers around the raw task. Way 0 draws with
// rdpq only. The ROM cycles through the ways and keeps its counts in rsp_results.

#include <libdragon.h>
#include <string.h>

// rspq_start loads a static overlay header that has 4-byte alignment, and a
// DMA needs 8. The link sends each call from outside rsp.c through a buffer.
void __real_rsp_load_data(void *start, unsigned long size, unsigned int dmem_offset);

void __wrap_rsp_load_data(void *start, unsigned long size, unsigned int dmem_offset)
{
    static uint64_t bounce[32] __attribute__((aligned(16)));
    if (((uint32_t)start & 7) && size <= sizeof bounce) {
        memcpy(bounce, start, size);
        data_cache_hit_writeback(bounce, sizeof bounce);
        start = bounce;
    }
    __real_rsp_load_data(start, size, dmem_offset);
}

#define W 320
#define H 240
#define SWITCHES 5
#define PHASE 300
#define WARM 4

// rsp_blob is ROM 0x40F80..0x56410 of the base ROM: the RSP boot, the
// microcodes and their data. These are the offsets of the boot and of u1.
extern uint8_t rsp_blob[];
#define BOOT 0x0
#define U1 0xD30
#define U1_DATA 0x14C50

#define SIGS_CLEAR 0xAAAA00
#define SIGS 0x7F80

extern volatile int __rspq_syncpoints_done[4];

enum { WAY0, WAY1, WAY2, WAY2_SIG, WAYS };
static const int phase_way[] = { WAY1, WAY2, WAY0, WAY2_SIG };
static const int phase_frames[] = { PHASE, PHASE, PHASE, 2 };

struct stats {
    uint32_t n, wait, out, raw, in, out_max, in_max;
    uint32_t irqs, taskdone, false_sync, timeouts;
};

struct results {
    uint32_t magic, frame, way, phase_frame;
    struct stats cur[WAYS], last[WAYS];
};
volatile struct results rsp_results = { .magic = 0x52535051 };

static uint16_t fb_mem[2][W * H] __attribute__((aligned(64)));
static uint64_t dram_stack[0x400 / 8] __attribute__((aligned(16)));
static uint64_t yield_buf[0xC00 / 8] __attribute__((aligned(16)));
static uint32_t dlist[SWITCHES][18] __attribute__((aligned(16)));
static uint32_t task[16] __attribute__((aligned(16)));
static uint64_t save_imem[4096 / 8] __attribute__((aligned(16)));
static uint64_t save_dmem[4096 / 8] __attribute__((aligned(16)));

static volatile int raw_running;
static volatile uint32_t raw_irqs, raw_taskdone;
static bool sig2_to_rspq;

// While the raw task runs, its break raises the SP interrupt. The task sets
// SIG2 as it ends, and rspq takes SIG2 as a syncpoint, so this clears it.
static void raw_sp_handler(void)
{
    if (!raw_running)
        return;
    uint32_t st = *SP_STATUS;
    if (!(st & SP_STATUS_BROKE))
        return;
    raw_irqs++;
    if (st & SP_STATUS_SIG2) {
        raw_taskdone++;
        if (!sig2_to_rspq)
            *SP_STATUS = SP_WSTATUS_CLEAR_SIG2;
    }
    raw_running = 0;
}

static bool wait_reg(volatile uint32_t *reg, uint32_t mask, uint32_t want)
{
    uint32_t end = TICKS_READ() + TICKS_FROM_MS(100);
    while ((*reg & mask) != want)
        if (!TICKS_BEFORE(TICKS_READ(), end))
            return false;
    return true;
}

// The display list in GBI words, and the OSTask at DMEM 0xFC0, as the game
// writes them. Each pointer is a physical address.
static void raw_build(int i, void *fb)
{
    uint32_t *d = dlist[i];
    uint32_t x0 = 16 + i * 60, x1 = x0 + 39, y0 = 84, y1 = 134;
    uint32_t words[18] = {
        0xE7000000, 0,                          // pipe sync
        0xFF10013F, PhysicalAddr(fb),           // colour image: RGBA16, 320 wide
        0xED000000, (W << 14) | (H << 2),       // scissor
        0xE3000A01, 0x00300000,                 // cycle type: fill
        0xF7000000, 0xF801F801,                 // fill colour: red
        0xF6000000 | (x1 << 14) | (y1 << 2), (x0 << 14) | (y0 << 2),
        0xE7000000, 0,                          // pipe sync
        0xE9000000, 0,                          // full sync
        0xDF000000, 0,                          // end of the list
    };
    memcpy(d, words, sizeof words);
    data_cache_hit_writeback(d, sizeof words);

    uint32_t t[16] = {
        1, 2,
        PhysicalAddr(rsp_blob + BOOT), 0xD0,
        PhysicalAddr(rsp_blob + U1), 0x1000,
        PhysicalAddr(rsp_blob + U1_DATA), 0x800,
        PhysicalAddr(dram_stack), sizeof dram_stack,
        0, 0,
        PhysicalAddr(d), sizeof words,
        PhysicalAddr(yield_buf), sizeof yield_buf,
    };
    memcpy(task, t, sizeof t);
    data_cache_hit_writeback(task, sizeof task);
}

// Loads the boot and the task, starts the RSP at IMEM 0, and waits for the
// break and for the full sync of the RDP.
static void raw_run(struct stats *s, int i, void *fb)
{
    raw_build(i, fb);
    *SP_STATUS = SIGS_CLEAR;
    rsp_load_code(rsp_blob + BOOT, 0xD0, 0);
    rsp_load_data(task, sizeof task, 0xFC0);
    int sync0 = __rspq_syncpoints_done[0];
    *MI_MODE = MI_WMODE_CLR_DPINT;
    raw_running = 1;
    rsp_run_async();
    uint32_t end = TICKS_READ() + TICKS_FROM_MS(100);
    while (raw_running || !(*MI_INTERRUPT & MI_INTERRUPT_DP)) {
        if (!TICKS_BEFORE(TICKS_READ(), end)) {
            s->timeouts++;
            raw_running = 0;
            break;
        }
    }
    *MI_MODE = MI_WMODE_CLR_DPINT;
    int extra = __rspq_syncpoints_done[0] - sync0;
    if (extra) {
        // Take back the syncpoints that rspq counted for the raw task, before
        // a wait of rspq returns too early on them.
        s->false_sync += extra;
        __rspq_syncpoints_done[0] -= extra;
        data_cache_hit_writeback((void *)__rspq_syncpoints_done, 4);
    }
}

// The RDP takes its commands from RDRAM again, from the end that rspq left.
static void dp_to_rdram(uint32_t end)
{
    *DP_STATUS = DP_WSTATUS_RESET_XBUS_DMEM_DMA | DP_WSTATUS_RESET_FLUSH | DP_WSTATUS_RESET_FREEZE;
    wait_reg(DP_STATUS, DP_STATUS_START_VALID | DP_STATUS_END_VALID, 0);
    *DP_START = end;
    *DP_END = end;
}

static void rdpq_fill(int i)
{
    int x0 = 16 + i * 60;
    rdpq_fill_rectangle(x0, 20, x0 + 40, 71);
}

static void maxw(uint32_t *m, uint32_t v)
{
    if (v > *m)
        *m = v;
}

static void one_switch(int way, struct stats *s, int i, surface_t *fb)
{
    uint32_t t0 = TICKS_READ();
    if (way == WAY1)
        rdpq_detach();  // rdpq_close takes no attached surface
    rspq_wait();
    uint32_t t1 = TICKS_READ();
    if (way == WAY0) {
        s->wait += t1 - t0;
        s->n++;
        return;
    }

    // rdpq's DP handler takes the full sync of rspq_wait first.
    wait_reg(SP_STATUS, SP_STATUS_SIG1, 0);
    uint32_t pc = 0, sigs = 0, dp_end = 0;
    if (way == WAY1) {
        // rdpq loses its attach and mode here, and rspq its queues.
        rdpq_close();
        rspq_close();
        set_SP_interrupt(1);
    } else {
        // rspq_wait returns before the RSP reaches its break.
        wait_reg(SP_STATUS, SP_STATUS_HALTED, SP_STATUS_HALTED);
        pc = *SP_PC;
        sigs = *SP_STATUS & SIGS;
        dp_end = *DP_END;
        rsp_read_code(save_imem, sizeof save_imem, 0);
        rsp_read_data(save_dmem, sizeof save_dmem, 0);
        // The full sync of the raw task must not reach rdpq's DP handler.
        set_DP_interrupt(0);
    }
    uint32_t t2 = TICKS_READ();

    uint32_t irqs = raw_irqs, done = raw_taskdone;
    raw_run(s, i, fb->buffer);
    s->irqs += raw_irqs - irqs;
    s->taskdone += raw_taskdone - done;
    uint32_t t3 = TICKS_READ();

    *SP_STATUS = SP_WSTATUS_CLEAR_INTR_BREAK;
    if (way == WAY1) {
        rspq_init();
        rdpq_init();
        rdpq_attach(fb, NULL);
        rdpq_set_mode_fill(RGBA32(0, 255, 0, 255));
    } else {
        dp_to_rdram(dp_end);
        rsp_load_code(save_imem, sizeof save_imem, 0);
        rsp_load_data(save_dmem, sizeof save_dmem, 0);
        *SP_PC = pc;
        uint32_t w = SIGS_CLEAR;
        for (int k = 0; k < 8; k++)
            if (sigs & (SP_STATUS_SIG0 << k))
                w = (w & ~(SP_WSTATUS_CLEAR_SIG0 << (2 * k))) | (SP_WSTATUS_SET_SIG0 << (2 * k));
        *SP_STATUS = w;
        set_DP_interrupt(1);
    }
    uint32_t t4 = TICKS_READ();

    s->n++;
    s->wait += t1 - t0;
    s->out += t2 - t1;
    s->raw += t3 - t2;
    s->in += t4 - t3;
    maxw(&s->out_max, t2 - t1);
    maxw(&s->in_max, t4 - t3);
}

static void print_stats(surface_t *fb, int y, const char *name, volatile struct stats *s)
{
    char line[64];
    uint32_t n = s->n ? s->n : 1;
    snprintf(line, sizeof line, "%s n%u w%u o%u r%u i%u", name, (unsigned)s->n,
             (unsigned)(s->wait / n), (unsigned)(s->out / n), (unsigned)(s->raw / n),
             (unsigned)(s->in / n));
    graphics_draw_text(fb, 8, y, line);
}

int main(void)
{
    vi_init();
    rdpq_init();
    register_SP_handler(raw_sp_handler);

    surface_t fbs[2];
    for (int k = 0; k < 2; k++)
        fbs[k] = surface_make_linear(UncachedAddr(fb_mem[k]), FMT_RGBA16, W, H);

    int phase = 0, phase_frame = 0;
    for (uint32_t frame = 0;; frame++) {
        int way = phase_way[phase];
        struct stats *s = (struct stats *)&rsp_results.cur[way];
        surface_t *fb = &fbs[frame & 1];
        if (phase_frame == 0) {
            memset(s, 0, sizeof *s);
            // The handler of the test ROM goes first, ahead of rspq's.
            unregister_SP_handler(raw_sp_handler);
            register_SP_handler(raw_sp_handler);
            sig2_to_rspq = way == WAY2_SIG;
        }
        rsp_results.frame = frame;
        rsp_results.way = way;
        rsp_results.phase_frame = phase_frame;

        struct stats warm;
        struct stats *count = phase_frame < WARM && way != WAY2_SIG ? memset(&warm, 0, sizeof warm) : s;
        rdpq_attach(fb, NULL);
        rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, W, H);
        rdpq_set_mode_fill(RGBA32(0, 255, 0, 255));
        for (int i = 0; i < SWITCHES; i++) {
            rdpq_fill(i);
            one_switch(way, count, i, fb);
        }
        rdpq_detach_wait();

        print_stats(fb, 150, "way0", &rsp_results.last[WAY0]);
        print_stats(fb, 160, "way1", &rsp_results.last[WAY1]);
        print_stats(fb, 170, "way2", &rsp_results.last[WAY2]);
        volatile struct stats *sg = &rsp_results.last[WAY2_SIG];
        char line[64];
        snprintf(line, sizeof line, "irq %u done %u sync %u to %u",
                 (unsigned)rsp_results.last[WAY1].irqs, (unsigned)rsp_results.last[WAY1].taskdone,
                 (unsigned)sg->false_sync, (unsigned)sg->timeouts);
        graphics_draw_text(fb, 8, 180, line);
        snprintf(line, sizeof line, "frame %u way %d", (unsigned)frame, way);
        graphics_draw_text(fb, 8, 190, line);
        vi_show(fb);
        vi_wait_vblank();

        if (++phase_frame == phase_frames[phase]) {
            memcpy((void *)&rsp_results.last[way], s, sizeof *s);
            phase_frame = 0;
            phase = (phase + 1) % 4;
        }
    }
}
