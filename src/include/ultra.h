// libultra's types and calls as the game passes them. Game code reads these
// structs in place, so each size and offset is libultra's. The calls run on
// libdragon's kernel (src/dragon/kernel/).

#ifndef ULTRA_H
#define ULTRA_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;

typedef s32 OSPri;
typedef s32 OSId;
typedef u32 OSIntMask;
typedef u32 OSEvent;
typedef u64 OSTime;
typedef void *OSMesg;

typedef union {
    struct { f32 f_odd, f_even; } f;
    f64 d;
} __OSfp;

typedef struct {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    __OSfp fp0, fp2, fp4, fp6, fp8, fp10, fp12, fp14;
    __OSfp fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;

typedef struct OSThread_s {
    struct OSThread_s *next;
    OSPri priority;
    struct OSThread_s **queue;
    struct OSThread_s *tlnext;
    u16 state;
    u16 flags;
    OSId id;
    int fp;
    void *thprof;  // the game's osCreateThread never writes it
    __OSThreadContext context;
} OSThread;

typedef struct OSMesgQueue_s {
    OSThread *mtqueue;
    OSThread *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;

typedef struct {
    u32 errStatus;
    void *dramAddr;
    void *C2Addr;
    u32 sectorSize;
    u32 C1ErrNum;
    u32 C1ErrSector[4];
} __OSBlockInfo;

typedef struct {
    u32 cmdType;
    u16 transferMode;
    u16 blockNum;
    s32 sectorNum;
    u32 devAddr;
    u32 bmCtlShadow;
    u32 seqCtlShadow;
    __OSBlockInfo block[2];
} __OSTranxInfo;

typedef struct OSPiHandle_s {
    struct OSPiHandle_s *next;
    u8 type;
    u8 latency;
    u8 pageSize;
    u8 relDuration;
    u8 pulse;
    u8 domain;
    u32 baseAddress;
    u32 speed;
    __OSTranxInfo transferInfo;
} OSPiHandle;

typedef struct {
    u16 type;
    u8 pri;
    u8 status;
    OSMesgQueue *retQueue;
} OSIoMesgHdr;

typedef struct {
    OSIoMesgHdr hdr;
    void *dramAddr;
    u32 devAddr;
    u32 size;
    OSPiHandle *piHandle;
} OSIoMesg;

typedef struct {
    u32 type;
    u32 flags;
    u64 *ucode_boot;
    u32 ucode_boot_size;
    u64 *ucode;
    u32 ucode_size;
    u64 *ucode_data;
    u32 ucode_data_size;
    u64 *dram_stack;
    u32 dram_stack_size;
    u64 *output_buff;
    u64 *output_buff_size;
    u64 *data_ptr;
    u32 data_size;
    u64 *yield_data_ptr;
    u32 yield_data_size;
} OSTask_t;

typedef union {
    OSTask_t t;
    long long force_structure_alignment;
} OSTask;

typedef struct {
    u32 ctrl, width, burst, vSync, hSync, leap, hStart, xScale, vCurrent;
} OSViCommonRegs;

typedef struct {
    u32 origin, yScale, vStart, vBurst, vIntr;
} OSViFieldRegs;

typedef struct {
    u8 type;
    OSViCommonRegs comRegs;
    OSViFieldRegs fldRegs[2];
} OSViMode;

typedef struct OSTimer_s {
    struct OSTimer_s *next;
    struct OSTimer_s *prev;
    OSTime interval;
    OSTime value;
    OSMesgQueue *mq;
    OSMesg msg;
} OSTimer;

#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1

#define OS_STATE_STOPPED 1
#define OS_STATE_RUNNABLE 2
#define OS_STATE_RUNNING 4
#define OS_STATE_WAITING 8

#define OS_PRIORITY_IDLE 0

#define OS_EVENT_COUNTER 3
#define OS_EVENT_SP 4
#define OS_EVENT_SI 5
#define OS_EVENT_AI 6
#define OS_EVENT_VI 7
#define OS_EVENT_PI 8
#define OS_EVENT_DP 9
#define OS_EVENT_SP_BREAK 11
#define OS_EVENT_PRENMI 14
#define OS_NUM_EVENTS 15

#define OS_IM_NONE 0x00000001
#define OS_IM_SP 0x00010401
#define OS_IM_SI 0x00020401
#define OS_IM_AI 0x00040401
#define OS_IM_VI 0x00080401
#define OS_IM_PI 0x00100401
#define OS_IM_DP 0x00200401
#define OS_IM_ALL 0x003FFF01

// Words that the IPL3 and libultra keep in low RAM.
#define osResetType (*(volatile s32 *)0x8000030C)
#define osAppNMIBuffer ((u8 *)0x8000031C)
#define OS_APP_NMI_BUFSIZE 64

// Each thread that osStartThread made, and entry 0 for the main kthread.
// An empty entry has os NULL.
#define ULTRA_THREADS 16
struct kthread_s;
struct ultra_thread {
    OSThread *os;
    struct kthread_s *k;
};
extern struct ultra_thread ultra_threads[ULTRA_THREADS];

void osInitialize(void);

void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri);
void osStartThread(OSThread *t);
void osDestroyThread(OSThread *t);
void osYieldThread(void);
void osSetThreadPri(OSThread *t, OSPri pri);
OSPri osGetThreadPri(OSThread *t);
struct kthread_s *ultra_kthread(OSThread *t);

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void osSetEventMesg(OSEvent event, OSMesgQueue *mq, OSMesg msg);
void ultra_event(int event);
void ultra_event_thread(int event);

OSIntMask osSetIntMask(OSIntMask im);
u32 __osDisableInt(void);
void __osRestoreInt(u32 old);

OSTime osGetTime(void);
u32 osGetCount(void);
int osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg);

#define ULTRA_SIZE(type, size) _Static_assert(sizeof(type) == (size), #type " size")
#define ULTRA_AT(type, field, at) _Static_assert(offsetof(type, field) == (at), #type "." #field)

ULTRA_SIZE(__OSThreadContext, 0x190);
ULTRA_AT(__OSThreadContext, a0, 0x18);
ULTRA_AT(__OSThreadContext, sp, 0xD0);
ULTRA_AT(__OSThreadContext, ra, 0xE0);
ULTRA_AT(__OSThreadContext, lo, 0xE8);
ULTRA_AT(__OSThreadContext, sr, 0xF8);
ULTRA_AT(__OSThreadContext, pc, 0xFC);
ULTRA_AT(__OSThreadContext, cause, 0x100);
ULTRA_AT(__OSThreadContext, rcp, 0x108);
ULTRA_AT(__OSThreadContext, fpcsr, 0x10C);
ULTRA_AT(__OSThreadContext, fp0, 0x110);
ULTRA_AT(__OSThreadContext, fp30, 0x188);

ULTRA_SIZE(OSThread, 0x1B0);
ULTRA_AT(OSThread, next, 0x00);
ULTRA_AT(OSThread, priority, 0x04);
ULTRA_AT(OSThread, queue, 0x08);
ULTRA_AT(OSThread, tlnext, 0x0C);
ULTRA_AT(OSThread, state, 0x10);
ULTRA_AT(OSThread, flags, 0x12);
ULTRA_AT(OSThread, id, 0x14);
ULTRA_AT(OSThread, fp, 0x18);
ULTRA_AT(OSThread, thprof, 0x1C);
ULTRA_AT(OSThread, context, 0x20);

ULTRA_SIZE(OSMesgQueue, 0x18);
ULTRA_AT(OSMesgQueue, mtqueue, 0x00);
ULTRA_AT(OSMesgQueue, fullqueue, 0x04);
ULTRA_AT(OSMesgQueue, validCount, 0x08);
ULTRA_AT(OSMesgQueue, first, 0x0C);
ULTRA_AT(OSMesgQueue, msgCount, 0x10);
ULTRA_AT(OSMesgQueue, msg, 0x14);

ULTRA_SIZE(OSMesg, 4);
ULTRA_SIZE(OSTime, 8);
ULTRA_SIZE(OSPri, 4);
ULTRA_SIZE(OSId, 4);
ULTRA_SIZE(OSIntMask, 4);

ULTRA_SIZE(__OSBlockInfo, 0x24);
ULTRA_SIZE(__OSTranxInfo, 0x60);
ULTRA_AT(__OSTranxInfo, block, 0x18);
ULTRA_SIZE(OSPiHandle, 0x74);
ULTRA_AT(OSPiHandle, type, 0x04);
ULTRA_AT(OSPiHandle, domain, 0x09);
ULTRA_AT(OSPiHandle, baseAddress, 0x0C);
ULTRA_AT(OSPiHandle, speed, 0x10);
ULTRA_AT(OSPiHandle, transferInfo, 0x14);

ULTRA_SIZE(OSIoMesgHdr, 0x08);
ULTRA_AT(OSIoMesgHdr, retQueue, 0x04);
ULTRA_SIZE(OSIoMesg, 0x18);
ULTRA_AT(OSIoMesg, dramAddr, 0x08);
ULTRA_AT(OSIoMesg, devAddr, 0x0C);
ULTRA_AT(OSIoMesg, size, 0x10);
ULTRA_AT(OSIoMesg, piHandle, 0x14);

ULTRA_SIZE(OSTask, 0x40);
ULTRA_AT(OSTask_t, ucode_boot, 0x08);
ULTRA_AT(OSTask_t, ucode, 0x10);
ULTRA_AT(OSTask_t, ucode_data, 0x18);
ULTRA_AT(OSTask_t, dram_stack, 0x20);
ULTRA_AT(OSTask_t, output_buff, 0x28);
ULTRA_AT(OSTask_t, data_ptr, 0x30);
ULTRA_AT(OSTask_t, yield_data_ptr, 0x38);
ULTRA_AT(OSTask_t, yield_data_size, 0x3C);

ULTRA_SIZE(OSViMode, 0x50);
ULTRA_AT(OSViMode, comRegs, 0x04);
ULTRA_AT(OSViMode, fldRegs, 0x28);
ULTRA_SIZE(OSViFieldRegs, 0x14);

ULTRA_SIZE(OSTimer, 0x20);
ULTRA_AT(OSTimer, interval, 0x08);
ULTRA_AT(OSTimer, value, 0x10);
ULTRA_AT(OSTimer, mq, 0x18);
ULTRA_AT(OSTimer, msg, 0x1C);

ULTRA_SIZE(struct ultra_thread, 8);

#endif
