// libultra's controller calls and raw SI call on libdragon's joybus. The
// game's rumble and Controller Pak routines call the raw call and its lock.

#include <libdragon.h>
#include <string.h>
#include "si.h"

// The words of the game that those routines also use. The split names each
// by its address in the retail image.
extern u8 D_8009ECD0[JOYBUS_BLOCK_SIZE];    // the PIF block
extern u8 D_800A8758;                       // the command of the last block
extern u8 D_800A10E4;                       // the count of ports in a block

#define CMD_STATUS 0
#define CMD_READ 1
#define CMD_NONE 0xFE

// The last block written, and whether joybus is busy with it.
static u8 block[JOYBUS_BLOCK_SIZE];
static volatile bool busy;

// In the SI interrupt, with the PIF's reply.
static void reply(uint64_t *out, void *buf)
{
    memcpy(buf, out, JOYBUS_BLOCK_SIZE);
    busy = false;
    ultra_event(OS_EVENT_SI);
}

// On libultra, each call is one SI transfer that ends in event 5. joybus
// sends a block and reads its reply in one exchange, so a write only keeps
// the block, and a read runs the exchange.
s32 __osSiRawStartDma(s32 dir, void *buf)
{
    if (busy)
        return -1;
    if (dir == OS_WRITE) {
        memcpy(block, buf, sizeof block);
        ultra_event_thread(OS_EVENT_SI);
    } else {
        busy = true;
        joybus_exec_async(block, reply, buf);
    }
    return 0;
}

// The lock that each user of the PIF block holds: a queue of one message.
static OSMesgQueue lock;
static OSMesg lock_msg;
static bool lock_made;

static void make_lock(void)
{
    lock_made = true;
    osCreateMesgQueue(&lock, &lock_msg, 1);
    osSendMesg(&lock, NULL, OS_MESG_NOBLOCK);
}

void __osSiGetAccess(void)
{
    if (!lock_made)
        make_lock();
    osRecvMesg(&lock, NULL, OS_MESG_BLOCK);
}

void __osSiRelAccess(void)
{
    osSendMesg(&lock, NULL, OS_MESG_NOBLOCK);
}

// Fills the PIF block with CMD for each port. A port takes 8 bytes: a pad
// byte, the counts of bytes to send and to receive, the command, and room
// for the reply.
static void pack(u8 cmd)
{
    u8 *p = D_8009ECD0;
    memset(p, 0, JOYBUS_BLOCK_SIZE);
    p[JOYBUS_BLOCK_SIZE - 1] = 1;
    for (int i = 0; i < D_800A10E4; i++, p += 8) {
        memset(p, 0xFF, 8);
        p[1] = 1;
        p[2] = cmd == CMD_READ ? 4 : 3;
        p[3] = cmd;
    }
    *p = 0xFE;
}

// The PIF sets the top two bits of the receive count when a port fails.
static u8 error(const u8 *p)
{
    return (p[2] & 0xC0) >> 4;
}

static void status(u8 *found, OSContStatus *st)
{
    const u8 *p = D_8009ECD0;
    u8 bits = 0;
    for (int i = 0; i < D_800A10E4; i++, p += 8, st++) {
        st->err = error(p);
        if (st->err)
            continue;
        st->type = p[5] << 8 | p[4];
        st->status = p[6];
        bits |= 1 << i;
    }
    *found = bits;
}

// Writes the block of CMD, unless the last block written was that one, and
// starts the read of its reply.
static s32 start(OSMesgQueue *mq, u8 cmd)
{
    __osSiGetAccess();
    if (D_800A8758 != cmd) {
        pack(cmd);
        __osSiRawStartDma(OS_WRITE, D_8009ECD0);
        osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    }
    s32 ret = __osSiRawStartDma(OS_READ, D_8009ECD0);
    D_800A8758 = cmd;
    __osSiRelAccess();
    return ret;
}

s32 osContInit(OSMesgQueue *mq, u8 *found, OSContStatus *st)
{
    static bool done;
    if (done)
        return 0;
    done = true;

    // A controller answers only half a second after power on.
    const OSTime ready = 23437500;
    OSTime now = osGetTime();
    if (now < ready) {
        OSMesgQueue q;
        OSMesg msg;
        OSTimer timer;
        osCreateMesgQueue(&q, &msg, 1);
        osSetTimer(&timer, ready - now, 0, &q, NULL);
        osRecvMesg(&q, NULL, OS_MESG_BLOCK);
    }

    D_800A10E4 = 4;
    pack(CMD_STATUS);
    __osSiRawStartDma(OS_WRITE, D_8009ECD0);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    s32 ret = __osSiRawStartDma(OS_READ, D_8009ECD0);
    osRecvMesg(mq, NULL, OS_MESG_BLOCK);
    status(found, st);
    D_800A8758 = CMD_STATUS;
    make_lock();
    return ret;
}

s32 osContSetCh(u8 ports)
{
    __osSiGetAccess();
    D_800A10E4 = ports > 4 ? 4 : ports;
    D_800A8758 = CMD_NONE;
    __osSiRelAccess();
    return 0;
}

s32 osContStartQuery(OSMesgQueue *mq)
{
    return start(mq, CMD_STATUS);
}

void osContGetQuery(OSContStatus *st)
{
    u8 found;
    status(&found, st);
}

s32 osContStartReadData(OSMesgQueue *mq)
{
    return start(mq, CMD_READ);
}

void osContGetReadData(OSContPad *pad)
{
    const u8 *p = D_8009ECD0;
    for (int i = 0; i < D_800A10E4; i++, p += 8, pad++) {
        pad->err = error(p);
        if (pad->err)
            continue;
        pad->button = p[4] << 8 | p[5];
        pad->stick_x = p[6];
        pad->stick_y = p[7];
    }
}

// The setup of the voice unit, which no run of the game reaches. 1 is
// libultra's error for a port with no device.
s32 osVoiceInit(void)
{
    return 1;
}
