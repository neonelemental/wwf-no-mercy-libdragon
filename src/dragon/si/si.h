// libultra's controller types and SI calls as the game passes them. Game code
// reads these structs in place, so each size and offset is libultra's.

#ifndef SI_H
#define SI_H

#include "ultra.h"

typedef struct {
    u16 type;
    u8 status;
    u8 err;
} OSContStatus;

typedef struct {
    u16 button;
    s8 stick_x;
    s8 stick_y;
    u8 err;
} OSContPad;

#define OS_READ 0
#define OS_WRITE 1

s32 osContInit(OSMesgQueue *mq, u8 *found, OSContStatus *st);
s32 osContSetCh(u8 ports);
s32 osContStartQuery(OSMesgQueue *mq);
void osContGetQuery(OSContStatus *st);
s32 osContStartReadData(OSMesgQueue *mq);
void osContGetReadData(OSContPad *pad);

// Starts a write of the 64 bytes at BUF to the PIF, or a read of them from
// it. Event 5 comes when it is done. Gives -1 when a read still runs.
s32 __osSiRawStartDma(s32 dir, void *buf);
void __osSiGetAccess(void);
void __osSiRelAccess(void);

ULTRA_SIZE(OSContStatus, 4);
ULTRA_AT(OSContStatus, status, 2);
ULTRA_AT(OSContStatus, err, 3);
ULTRA_SIZE(OSContPad, 6);
ULTRA_AT(OSContPad, stick_x, 2);
ULTRA_AT(OSContPad, err, 4);

#endif
