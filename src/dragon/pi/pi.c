// libultra's PI manager and PI calls on libdragon's DMA. A thread takes each
// request from the game's queue and runs it as one libdragon transfer. Each
// user of the PI holds the access queue, so one transfer or one IO runs at a
// time.

#include <libdragon.h>
#include <stdbool.h>
#include "ultra.h"

#define OS_READ 0
#define OS_MESG_TYPE_EDMAREAD 15
#define OS_MESG_TYPE_EDMAWRITE 16
#define OS_MESG_PRI_HIGH 1

#define DEVICE_TYPE_CART 0
#define DEVICE_TYPE_BULK 1

// Words of the game. The split names each by its address in the retail image.
extern OSPiHandle *D_8005343C;      // the list of handles, which osEPiLinkHandle also reads
extern OSPiHandle *D_80053440[2];   // for each domain, the type and timing that its registers hold

static OSMesgQueue *commands;
static OSThread thread;
static OSMesgQueue access;
static OSMesg access_msg;
static bool access_made;

void __osPiGetAccess(void)
{
    if (!access_made) {
        access_made = true;
        osCreateMesgQueue(&access, &access_msg, 1);
        osSendMesg(&access, NULL, OS_MESG_NOBLOCK);
    }
    osRecvMesg(&access, NULL, OS_MESG_BLOCK);
}

void __osPiRelAccess(void)
{
    osSendMesg(&access, NULL, OS_MESG_NOBLOCK);
}

// A write by the CPU keeps the PI busy and ends with no interrupt, which
// libdragon would wait for. So wait here, as libultra does.
static void idle(void)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
}

// libdragon never writes the timing registers of a domain. As libultra does,
// write them when the type of the handle is not the one that they hold.
static void timing(OSPiHandle *h)
{
    OSPiHandle *now = D_80053440[h->domain];
    if (now->type == h->type)
        return;
    volatile u32 *regs = (u32 *)(h->domain ? 0xA4600024 : 0xA4600014);
    if (now->latency != h->latency)
        regs[0] = h->latency;
    if (now->pulse != h->pulse)
        regs[1] = h->pulse;
    if (now->pageSize != h->pageSize)
        regs[2] = h->pageSize;
    if (now->relDuration != h->relDuration)
        regs[3] = h->relDuration;
    now->type = h->type;
    now->latency = h->latency;
    now->pulse = h->pulse;
    now->pageSize = h->pageSize;
    now->relDuration = h->relDuration;
}

static u32 bus(OSPiHandle *h, u32 addr)
{
    return (h->baseAddress | addr) & 0x1FFFFFFF;
}

s32 osEPiRawReadIo(OSPiHandle *h, u32 addr, u32 *data)
{
    idle();
    timing(h);
    *data = io_read(bus(h, addr));
    return 0;
}

s32 osEPiRawWriteIo(OSPiHandle *h, u32 addr, u32 data)
{
    idle();
    timing(h);
    io_write(bus(h, addr), data);
    return 0;
}

// osEPiStartDma is the only sender, so each request is a read or a write
// through a handle.
static void manager(void *arg)
{
    for (;;) {
        OSIoMesg *mb;
        osRecvMesg(commands, (OSMesg *)&mb, OS_MESG_BLOCK);
        OSPiHandle *h = mb->piHandle;
        __osPiGetAccess();
        idle();
        timing(h);
        u64 ticket = mb->hdr.type == OS_MESG_TYPE_EDMAREAD
            ? dma_read_raw_async(mb->dramAddr, bus(h, mb->devAddr), mb->size)
            : dma_write_raw_async(mb->dramAddr, bus(h, mb->devAddr), mb->size);
        dma_wait_finished(ticket);
        osSendMesg(mb->hdr.retQueue, mb, OS_MESG_NOBLOCK);
        __osPiRelAccess();
    }
}

// The thread runs on its own kthread stack.
void osCreatePiManager(OSPri pri, OSMesgQueue *mq, OSMesg *msg, s32 count)
{
    if (commands)
        return;
    osCreateMesgQueue(mq, msg, count);
    commands = mq;
    osCreateThread(&thread, 0, manager, NULL, NULL, pri);
    osStartThread(&thread);
}

s32 osEPiStartDma(OSPiHandle *h, OSIoMesg *mb, s32 direction)
{
    if (!commands)
        return -1;
    mb->piHandle = h;
    mb->hdr.type = direction == OS_READ ? OS_MESG_TYPE_EDMAREAD : OS_MESG_TYPE_EDMAWRITE;
    if (mb->hdr.pri == OS_MESG_PRI_HIGH)
        return osJamMesg(commands, mb, OS_MESG_NOBLOCK);
    return osSendMesg(commands, mb, OS_MESG_NOBLOCK);
}

// Fills a new handle of domain 1, and puts it on the list. Its timing is in
// the first word of its device, which is read at the slowest timing.
static void init(OSPiHandle *h, u8 type, u32 base)
{
    h->type = type;
    h->baseAddress = base;

    idle();
    volatile u32 *regs = (u32 *)0xA4600014;
    u32 old[4] = { regs[0], regs[1], regs[2], regs[3] };
    regs[0] = 0xFF;
    regs[1] = 0xFF;
    regs[2] = 0;
    regs[3] = 3;
    u32 w = io_read(bus(h, 0));
    for (int i = 0; i < 4; i++)
        regs[i] = old[i];
    h->latency = w;
    h->pulse = w >> 8;
    h->pageSize = (w >> 16) & 0xF;
    h->relDuration = (w >> 20) & 0xF;

    disable_interrupts();
    h->next = D_8005343C;
    D_8005343C = h;
    enable_interrupts();
}

static OSPiHandle *handle(OSPiHandle *h, u8 type, u32 base)
{
    __osPiGetAccess();
    if (!h->baseAddress)
        init(h, type, base);
    __osPiRelAccess();
    return h;
}

OSPiHandle *osCartRomInit(void)
{
    static OSPiHandle cart;
    return handle(&cart, DEVICE_TYPE_CART, 0xB0000000);
}

OSPiHandle *osDriveRomInit(void)
{
    static OSPiHandle drive;
    return handle(&drive, DEVICE_TYPE_BULK, 0xA6000000);
}
