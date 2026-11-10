/* The libdragon segment of the source variant, linked after the script of
 * the split. Its code and data go at rom 0x2000000, past the retail image,
 * and run in the Expansion Pak, in RAM that no mode of the game writes.
 * boot.S loads it there by PI DMA and clears its bss.
 *
 * The sections and the names follow libdragon's n64.ld, for the code that
 * _start needs.
 */

EXTERN (_start)

/* The RAM of the pak for libdragon, from 0x8043B810: its code, data, bss,
 * heap and stacks. */
dragon_end = 0x805BCCD0;

SECTIONS
{
    dragon_ROM_START = 0x2000000;
    dragon_VRAM = ADDR(.dragon);
    /* MAX lets the link end, so the check of the Makefile gives both numbers. */
    .dragon 0x8043B810 : AT(MAX(dragon_ROM_START, files_ROM_END))
    {
        __text_start = .;
        *(EXCLUDE_FILE(*/split/*) .boot)
        . = ALIGN(16);
        /* What a switch of thread and an interrupt run, side by side: it is
         * under 16 KB, so no two of its lines share a line of the
         * instruction cache. */
        *libdragon.a:inthandler.o(.text)
        *(.text.__onSyscallException .text.__kthread_syscall_schedule)
        *(.text.__kthread_check_overflow .text.__thlist_pop)
        *(.text.kcond_wait .text.kcond_signal .text.kthread_yield)
        *(.text.__kcond_signal_isr .text.__kcond_broadcast_isr)
        *(.text.kthread_current .text.disable_interrupts .text.enable_interrupts)
        *(.text.__MI_handler .text.get_ticks)
        */dragon/kernel/*.o(.text)
        *(EXCLUDE_FILE(*/split/*) .text .text.*)
        KEEP(*(keep.text.*))
        *(.init)
        *(.fini)
        *(.gnu.linkonce.t.*)
        . = ALIGN(16);
        __text_end = .;

        . = ALIGN(32);
        __intvectors_start = .;
        KEEP(*(.intvectors))
        __intvectors_end = .;

        __EH_FRAME_BEGIN__ = .;
        KEEP(*(.eh_frame))
        *(.gcc_except_table*)

        *(.rdata)
        *(EXCLUDE_FILE(*/split/*) .rodata .rodata.*)
        KEEP(*(keep.rodata.*))
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
        __tdata_align = .;
        LONG(ALIGNOF(.tdata));
        . = ALIGN(8);

        . = ALIGN(4);
        __CTOR_LIST__ = .;
        KEEP(*(.ctors))
        KEEP(*(SORT(.ctors.*)))
        __CTOR_END__ = .;

        . = ALIGN(8);
        __data_start = .;
        *(EXCLUDE_FILE(*/split/*) .data .data.*)
        KEEP(*(keep.data.*))
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);

        _gp = . + 0x8000;
        *(.sdata .sdata.*)
        *(.lit8)
        *(.lit4)
        . = ALIGN(16);
        __data_end = .;
    }
    dragon_VRAM_END = .;

    .tdata : AT(LOADADDR(.dragon) + SIZEOF(.dragon))
    {
        __tls_base = .;
        __tdata_start = .;
        *(.tdata .tdata.*)
        __tdata_end = .;
    }
    dragon_ROM_END = LOADADDR(.dragon) + SIZEOF(.dragon) + SIZEOF(.tdata);
    dragon_ROM_SIZE = dragon_ROM_END - LOADADDR(.dragon);

    /* A line of the D-cache never holds bytes of the DMA and of the bss. */
    .dragon_bss ALIGN(16) (NOLOAD) :
    {
        dragon_BSS_START = .;
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.scommon .scommon.*)
        *(EXCLUDE_FILE(*/split/*) .bss .bss.*)
        __tbss_start = .;
        *(.tbss .tbss.*)
        __tbss_end = .;
        __tls_end = .;
        *(COMMON)
        . = ALIGN(8);
        __th_tdata_copy = .;
        . = . + (__tdata_end - __tdata_start);
        . = ALIGN(16);
        __bss_end = .;
        dragon_BSS_END = .;
    }
    end = .;

    /DISCARD/ :
    {
        *(*);
    }

    ASSERT(dragon_BSS_END <= dragon_end, "libdragon passes dragon_end")
}
