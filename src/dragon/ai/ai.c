// libultra's AI calls on the AI registers. libdragon's audio.c takes no
// buffer from a caller, so these write the registers themselves.

#include "ultra.h"

#define AI_DRAM_ADDR (*(volatile u32 *)0xA4500000)
#define AI_LEN       (*(volatile u32 *)0xA4500004)
#define AI_STATUS    (*(volatile u32 *)0xA450000C)
#define AI_DACRATE   (*(volatile u32 *)0xA4500010)
#define AI_BITRATE   (*(volatile u32 *)0xA4500014)

#define AI_STATUS_FULL 0x80000000

extern s32 D_80054AF8;  // osViClock, which osInitialize fills

u32 osVirtualToPhysical(void *addr);

u32 osAiGetLength(void)
{
    return AI_LEN;
}

u32 osAiGetStatus(void)
{
    return AI_STATUS;
}

// Gives the rate that the DAC plays at, which is not quite FREQUENCY, or -1
// for a rate that the DAC cannot play.
s32 osAiSetFrequency(u32 frequency)
{
    u32 rate = (u32)((f32)D_80054AF8 / (f32)frequency + 0.5f);
    if (rate < 0x84)
        return -1;
    u8 bits = rate / 66;
    if (bits > 16)
        bits = 16;
    AI_DACRATE = rate - 1;
    AI_BITRATE = bits - 1;
    return D_80054AF8 / (s32)rate;
}

// Gives -1 while the AI holds two buffers.
s32 osAiSetNextBuffer(void *buf, u32 size)
{
    // The AI starts a buffer 0x2000 too high when the one before it ended on
    // a 0x2000 boundary.
    static u8 ended_on_boundary;
    u8 *start = buf;

    if (AI_STATUS & AI_STATUS_FULL)
        return -1;
    if (ended_on_boundary)
        start -= 0x2000;
    ended_on_boundary = (((u32)buf + size) & 0x1FFF) == 0;
    AI_DRAM_ADDR = osVirtualToPhysical(start);
    AI_LEN = size;
    return 0;
}
