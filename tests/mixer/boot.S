# The code at 0x80000400 that the stock IPL3 jumps to. It does what libdragon's
# IPL3 does before libdragon's _start, from the boot words of the stock IPL3.

	.set noreorder
	.set noat
	.section .stockboot, "ax"
	.global boot_entry
boot_entry:
	# Clear bss, as the stock IPL3 loads 1 MiB of ROM over it.
	la $8, __bss_start
	la $9, __bss_end
1:	sw $0, 0($8)
	addiu $8, 4
	sltu $1, $8, $9
	bnez $1, 1b
	nop

	# Boot words of the stock IPL3: TV type, reset type and memory size.
	lui $8, 0x8000
	lw $16, 0x300($8)
	lw $17, 0x30C($8)
	lw $18, 0x318($8)

	# Stack at the end of RDRAM less 16 bytes.
	addu $29, $8, $18
	addiu $29, -16

	# Copy the 0x200 bytes of the vectors to 0x80000000.
	la $9, __intvectors_start
	move $10, $8
	addiu $11, $8, 0x200
2:	lw $1, 0($9)
	sw $1, 0($10)
	addiu $9, 4
	addiu $10, 4
	bne $10, $11, 2b
	nop

	# Write back the D-cache and invalidate the I-cache over the vectors.
	move $10, $8
3:	cache 0x19, 0($10)
	cache 0x10, 0($10)
	addiu $10, 16
	bne $10, $11, 3b
	nop

	# Set bit 3 of the last PIF RAM word, as osInitialize does, so the PIF
	# does not reset the console.
	lui $12, 0xA480
	lui $13, 0xBFC0
4:	lw $1, 0x18($12)
	andi $1, 3
	bnez $1, 4b
	nop
	lw $14, 0x7FC($13)
5:	lw $1, 0x18($12)
	andi $1, 3
	bnez $1, 5b
	nop
	ori $14, 8
	sw $14, 0x7FC($13)
	# The write ends with an SI interrupt, which the joybus code refuses when
	# it sent nothing. Wait for the SI and clear the interrupt.
6:	lw $1, 0x18($12)
	andi $1, 3
	bnez $1, 6b
	nop
	sw $0, 0x18($12)

	# Boot flags in DMEM, as libdragon's IPL3 writes them. DMEM takes words only.
	# Word 0: memory size. Word 1: entropy. Word 2: ROM type, TV type, reset
	# type and console type, a byte each. Word 3: ELF offset, not used.
	lui $12, 0xA400
	sw $18, 0($12)
	sw $0, 4($12)
	andi $16, 0xFF
	sll $16, 16
	andi $17, 0xFF
	sll $17, 8
	or $16, $17
	sw $16, 8($12)
	sw $0, 12($12)

	j _start
	nop
