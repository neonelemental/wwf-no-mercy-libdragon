// libultra's RSP task calls. libdragon's rsp.c loads and starts what the game
// gives it. rsp_init is not called: the SP interrupt stays with the kernel's
// handler, and libdragon's command queue never starts.

#include <libdragon.h>
#include <string.h>
#include "ultra.h"

#define OS_TASK_YIELDED 1
#define OS_TASK_DP_WAIT 2
#define OS_TASK_LOADABLE 4
#define OS_YIELD_DATA_SIZE 0xC00

// The microcode's boot reads the task from here.
#define TASK_DMEM 0xFC0

static OSTask task __attribute__((aligned(16)));

static u64 *phys(u64 *p)
{
    return (u64 *)PhysicalAddr(p);
}

s32 __osSpSetPc(u32 pc)
{
    if (!(*SP_STATUS & SP_STATUS_HALTED))
        return -1;
    *SP_PC = pc;
    return 0;
}

void osSpTaskLoad(OSTask *in)
{
    OSTask_t *t = &task.t;
    memcpy(t, in, sizeof task);
    // The microcode takes each pointer but the boot's as a physical address.
    t->ucode = phys(t->ucode);
    t->ucode_data = phys(t->ucode_data);
    t->dram_stack = phys(t->dram_stack);
    t->output_buff = phys(t->output_buff);
    t->output_buff_size = phys(t->output_buff_size);
    t->data_ptr = phys(t->data_ptr);
    t->yield_data_ptr = phys(t->yield_data_ptr);

    if (t->flags & OS_TASK_YIELDED) {
        // A task that yielded goes on from the data that it saved, which the
        // RSP wrote past the cache.
        t->ucode_data = t->yield_data_ptr;
        t->ucode_data_size = t->yield_data_size;
        in->t.flags &= ~OS_TASK_YIELDED;
        if (t->flags & OS_TASK_LOADABLE)
            t->ucode = *(u64 **)UncachedAddr((u8 *)in->t.yield_data_ptr + OS_YIELD_DATA_SIZE - 4);
    }
    data_cache_hit_writeback(&task, sizeof task);

    *SP_STATUS = SP_WSTATUS_CLEAR_SIG0 | SP_WSTATUS_CLEAR_SIG1 | SP_WSTATUS_CLEAR_SIG2 | SP_WSTATUS_SET_INTR_BREAK;
    while (__osSpSetPc(0) == -1) {}
    rsp_load_data(&task, sizeof task, TASK_DMEM);
    rsp_load_code(t->ucode_boot, t->ucode_boot_size, 0);
}

// rsp.c starts the RSP at the first word of IMEM, with the interrupt on break.
void osSpTaskStartGo(OSTask *t)
{
    rsp_run_async();
}

void osSpTaskYield(void)
{
    *SP_STATUS = SP_WSTATUS_SET_SIG0;
}

// SIG0 asks for the yield and SIG1 says that the task yielded.
u32 osSpTaskYielded(OSTask *t)
{
    u32 status = *SP_STATUS;
    u32 yielded = (status & SP_STATUS_SIG1) != 0;
    if (status & SP_STATUS_SIG0) {
        t->t.flags |= yielded;
        t->t.flags &= ~OS_TASK_DP_WAIT;
    }
    return yielded;
}
