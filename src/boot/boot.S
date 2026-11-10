# The code at 0x80000400 that the stock IPL3 jumps to, in place of the retail
# entry. It clears main bss, loads the libdragon segment from the ROM into the
# Expansion Pak and clears its bss. Then it goes on to dragon_start there.
# With no pak, it stops.
	.set noreorder
	.set noat
	.section .text, "ax"

	.global boot
	.type boot, @function
boot:
	la	$t0, main_BSS_START
	la	$t1, main_BSS_END
	beq	$t0, $t1, 2f
	nop
1:	sw	$zero, 0($t0)
	addiu	$t0, $t0, 8
	bne	$t0, $t1, 1b
	sw	$zero, -4($t0)
2:
	# The IPL3 writes the RDRAM size at 0x80000318. Under 8 MB there is no pak,
	# and the console stays here.
	lui	$t0, 0x8000
	lw	$t1, 0x318($t0)
	lui	$t0, 0x80
	sltu	$t1, $t1, $t0
3:	bnez	$t1, 3b
	nop

	# PI DMA of the segment. The boot DMA of the IPL3 is over, as the CPU runs
	# the code that it loaded.
	lui	$t0, 0xA460
	la	$t1, dragon_VRAM
	sll	$t1, $t1, 3
	srl	$t1, $t1, 3
	sw	$t1, 0x0($t0)
	la	$t1, dragon_ROM_START
	lui	$t3, 0x1000
	or	$t1, $t1, $t3
	sw	$t1, 0x4($t0)
	la	$t1, dragon_ROM_SIZE
	addiu	$t1, $t1, -1
	sw	$t1, 0xC($t0)
4:	lw	$t1, 0x10($t0)
	andi	$t1, $t1, 3
	bnez	$t1, 4b
	nop
	# Clear the interrupt of the transfer, which nothing waits on.
	ori	$t1, $zero, 2
	sw	$t1, 0x10($t0)

	la	$t0, dragon_BSS_START
	la	$t1, dragon_BSS_END
	beq	$t0, $t1, 6f
	nop
5:	sw	$zero, 0($t0)
	addiu	$t0, $t0, 8
	bne	$t0, $t1, 5b
	sw	$zero, -4($t0)
6:
	la	$t2, dragon_start
	jr	$t2
	nop
	.size boot, . - boot

	# Main starts at 0x80000400 plus this size. 0xD0 keeps main at its retail
	# place modulo 0x40, as a pad does.
	.org 0xD0
