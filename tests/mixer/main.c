// Plays VADPCM samples of the base ROM on libdragon's mixer with 0, 1, 8, 16
// and 24 channels, each at its root note at ratio 1, and times each poll with the
// Count register: mixer_poll_async alone, then up to the end of the RSP work.
// The ROM cycles through the phases and keeps its counts in mixer_results.

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

void mixer_poll_async(int16_t *out, int nsamples);

struct sample_row { int index, at, frames; };
#include "build/samples.h"
extern uint8_t sample_blob[];

#define CHANNELS 24
#define WARM 60
#define MEASURE 600
static const int phase_channels[] = { 0, 1, 8, 16, 24 };
#define PHASES 5

typedef struct {
    void *codebook;
    void *loop_state;
    uint8_t bits;
} vadpcm_codec_t;

struct phase {
    uint32_t channels, vis, polls, samples;
    uint32_t cpu, all, cpu_max, all_max;
    uint32_t nonzero, peak, starts;
};

struct results {
    uint32_t magic, vi, phase, phase_vi, rate, buffer;
    struct phase cur, last[PHASES];
};
volatile struct results mixer_results = { .magic = 0x4D495852 };

static waveform_t waves[SAMPLES];
static vadpcm_codec_t codecs[SAMPLES];

// The mixer seeks a resident sample by its codec state. Each sample here has
// no loop, so the only seek is to the start, with a clear state.
static void seek_start(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)ctx; (void)wlen;
    if (seeking && wpos == 0)
        memset(sbuf->state, 0, sbuf->state_size);
}

static void make_waves(int rate)
{
    for (int i = 0; i < SAMPLES; i++) {
        const struct sample_row *r = &sample_rows[i];
        uint8_t *book = sample_blob + r->at;
        data_cache_hit_writeback(book, 136 + r->frames * 9);
        codecs[i] = (vadpcm_codec_t){ .codebook = book, .bits = 4 };
        waves[i] = (waveform_t){
            .name = "sample",
            .bits = 16,
            .channels = 1,
            .format = WAVEFORM_FORMAT_VADPCM,
            .frequency = rate,
            .len = r->frames * 16,
            .read = seek_start,
            .state_size = 16,
            .mem = book + 136,
            .codec = &codecs[i],
        };
    }
}

static void maxw(volatile uint32_t *m, uint32_t v)
{
    if (v > *m)
        *m = v;
}

static void poll_once(volatile struct phase *p)
{
    int n = audio_get_buffer_length();
    int16_t *buf = audio_write_begin();
    uint32_t t0 = TICKS_READ();
    mixer_poll_async(buf, n);
    uint32_t t1 = TICKS_READ();
    rspq_highpri_sync();
    uint32_t t2 = TICKS_READ();
    // The AI buffers are uncached, so the CPU reads what the RSP wrote.
    for (int i = 0; i < n * 2; i++) {
        int v = buf[i] < 0 ? -buf[i] : buf[i];
        if (v) {
            p->nonzero++;
            maxw(&p->peak, v);
        }
    }
    audio_write_end();
    p->polls++;
    p->samples += n;
    p->cpu += t1 - t0;
    p->all += t2 - t0;
    maxw(&p->cpu_max, t1 - t0);
    maxw(&p->all_max, t2 - t0);
}

static void print_phase(surface_t *fb, int y, volatile struct phase *p)
{
    char line[80];
    uint32_t v = p->vis ? p->vis : 1;
    // Count ticks a VI, and ms a VI at 48,585 ticks a ms, in thousandths.
    uint32_t cpu = p->cpu / v, all = p->all / v;
    snprintf(line, sizeof line, "%2u cpu %5u %u.%03u all %5u %u.%03u", (unsigned)p->channels,
             (unsigned)cpu, (unsigned)(cpu / 48585), (unsigned)(cpu % 48585 * 1000 / 48585),
             (unsigned)all, (unsigned)(all / 48585), (unsigned)(all % 48585 * 1000 / 48585));
    graphics_draw_text(fb, 8, y, line);
}

static uint16_t fb_mem[320 * 240] __attribute__((aligned(64)));

int main(void)
{
    vi_init();
    audio_init(28805, AUDIO_DEFAULT_LATENCY);
    mixer_init(CHANNELS);
    int rate = audio_get_frequency();
    mixer_results.rate = rate;
    mixer_results.buffer = audio_get_buffer_length();
    make_waves(rate);
    for (int ch = 0; ch < CHANNELS; ch++)
        mixer_ch_set_vol(ch, 0.1f, 0.1f);

    surface_t fb = surface_make_linear(UncachedAddr(fb_mem), FMT_RGBA16, 320, 240);
    graphics_fill_screen(&fb, 0);
    vi_show(&fb);

    int phase = 0, phase_vi = 0;
    for (uint32_t vi = 0;; vi++) {
        int nch = phase_channels[phase];
        volatile struct phase *p = &mixer_results.cur;
        if (phase_vi == 0) {
            for (int ch = 0; ch < CHANNELS; ch++)
                mixer_ch_stop(ch);
        }
        if (phase_vi == WARM) {
            memset((void *)p, 0, sizeof *p);
            p->channels = nch;
        }
        mixer_results.vi = vi;
        mixer_results.phase = phase;
        mixer_results.phase_vi = phase_vi;

        for (int ch = 0; ch < nch; ch++)
            if (!mixer_ch_playing(ch)) {
                mixer_ch_play(ch, &waves[(ch + p->starts) % SAMPLES]);
                p->starts++;
            }
        while (audio_can_write())
            poll_once(p);
        if (phase_vi >= WARM)
            p->vis++;

        if (vi % 60 == 0) {
            graphics_fill_screen(&fb, 0);
            graphics_draw_text(&fb, 8, 16, "channels, ticks and ms a VI");
            for (int k = 0; k < PHASES; k++)
                print_phase(&fb, 32 + 10 * k, &mixer_results.last[k]);
            char line[64];
            snprintf(line, sizeof line, "rate %d buffer %d vi %u", rate, (int)mixer_results.buffer,
                     (unsigned)vi);
            graphics_draw_text(&fb, 8, 90, line);
        }
        vi_wait_vblank();

        if (++phase_vi == WARM + MEASURE) {
            memcpy((void *)&mixer_results.last[phase], (void *)p, sizeof *p);
            phase_vi = 0;
            phase = (phase + 1) % PHASES;
        }
    }
}
