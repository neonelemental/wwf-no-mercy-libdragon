# The way into libdragon, in the Expansion Pak. It does what libdragon's IPL3
# does before _start, from the boot words of the stock IPL3.
	.set noreorder
	.set noat
	.section .text, "ax"

	.global dragon_start
	.type dragon_start, @function
dragon_start:
	# Copy the 0x200 bytes of libdragon's vectors to 0x80000000, then write
	# back the D-cache and invalidate the I-cache over them.
	lui	$t0, 0x8000
	la	$t1, __intvectors_start
	move	$t2, $t0
	addiu	$t3, $t0, 0x200
1:	lw	$at, 0($t1)
	sw	$at, 0($t2)
	addiu	$t2, $t2, 4
	bne	$t2, $t3, 1b
	addiu	$t1, $t1, 4
	move	$t2, $t0
2:	cache	0x19, 0($t2)
	cache	0x10, 0($t2)
	addiu	$t2, $t2, 16
	bne	$t2, $t3, 2b
	nop

	# libdragon's RAM ends at dragon_end: its stack is under that address, and
	# its heap under the stack.
	la	$sp, dragon_end
	addiu	$sp, $sp, -16

	# The boot flags in DMEM, as libdragon's IPL3 writes them. Word 0: the
	# memory size. Word 1: entropy. Word 2: the TV type and the reset type, a
	# byte each, from the boot words of the stock IPL3.
	lui	$t1, 0xA400
	la	$t2, dragon_end
	sll	$t2, $t2, 3
	srl	$t2, $t2, 3
	sw	$t2, 0($t1)
	sw	$zero, 4($t1)
	lw	$t2, 0x300($t0)
	lw	$t3, 0x30C($t0)
	andi	$t2, $t2, 0xFF
	sll	$t2, $t2, 16
	andi	$t3, $t3, 0xFF
	sll	$t3, $t3, 8
	or	$t2, $t2, $t3
	sw	$t2, 8($t1)
	sw	$zero, 12($t1)

	j	_start
	nop
	.size dragon_start, . - dragon_start
