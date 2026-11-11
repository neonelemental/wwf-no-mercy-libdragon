# The registers of a thread across a switch. libdragon saves a thread in one
# of two frames: every register when an interrupt takes the CPU from it, and
# only what a call keeps when it gives up the CPU itself. Each routine here
# holds a pattern of its own in each register that its frame must keep, and
# counts the registers that hold another value afterwards.
	.set noreorder
	.set noat
	.text

	.macro	pattern reg, base, n
	dli	\reg, \base + \n * 0x100000001
	.endm

	.macro	differs reg, base, n, scratch, count
	pattern	\scratch, \base, \n
	beq	\reg, \scratch, 1f
	nop
	addiu	\count, \count, 1
1:
	.endm

	.macro	frame
	addiu	$sp, $sp, -240
	sd	$16, 32($sp)
	sd	$17, 40($sp)
	sd	$18, 48($sp)
	sd	$19, 56($sp)
	sd	$20, 64($sp)
	sd	$21, 72($sp)
	sd	$22, 80($sp)
	sd	$23, 88($sp)
	sd	$30, 96($sp)
	sd	$31, 104($sp)
	.irp	n, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	sdc1	$f\n, (112 + (\n - 20) * 8)($sp)
	.endr
	cfc1	$8, $31
	sd	$8, 208($sp)
	.endm

	.macro	leave
	ld	$8, 208($sp)
	ctc1	$8, $31
	.irp	n, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	ldc1	$f\n, (112 + (\n - 20) * 8)($sp)
	.endr
	ld	$16, 32($sp)
	ld	$17, 40($sp)
	ld	$18, 48($sp)
	ld	$19, 56($sp)
	ld	$20, 64($sp)
	ld	$21, 72($sp)
	ld	$22, 80($sp)
	ld	$23, 88($sp)
	ld	$30, 96($sp)
	ld	$31, 104($sp)
	jr	$31
	addiu	$sp, $sp, 240
	.endm

# u32 NAME(volatile u32 *turn): holds a pattern in every register but s0 to
# s2, k0, k1, gp and sp, until another thread changes *turn. hi is 32, lo 33,
# and an FPU register 40 and up.
	.macro	spin name, base, fcr
	.globl	\name
\name:
	frame
	move	$16, $4
	lw	$17, 0($16)
	li	$8, \fcr
	ctc1	$8, $31
	.irp	n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	pattern	$8, \base, 40 + \n
	dmtc1	$8, $f\n
	.endr
	pattern	$8, \base, 32
	mthi	$8
	pattern	$8, \base, 33
	mtlo	$8
	.irp	n, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 19, 20, 21, 22, 23, 24, 25, 30, 31
	pattern	$\n, \base, \n
	.endr
2:	lw	$18, 0($16)
	beq	$18, $17, 2b
	nop
	move	$17, $0
	.irp	n, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 19, 20, 21, 22, 23, 24, 25, 30, 31
	differs	$\n, \base, \n, $18, $17
	.endr
	mfhi	$8
	differs	$8, \base, 32, $18, $17
	mflo	$8
	differs	$8, \base, 33, $18, $17
	.irp	n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	dmfc1	$8, $f\n
	differs	$8, \base, 40 + \n, $18, $17
	.endr
	cfc1	$8, $31
	li	$18, \fcr
	beq	$8, $18, 1f
	nop
	addiu	$17, $17, 1
1:	move	$2, $17
	leave
	.endm

# The rounding mode and the flush bit of FCR31 are part of each pattern.
	spin	regs_spin_a, 0x1A1A0000A1A10000, 0x01000001
	spin	regs_spin_b, 0x2B2B0000B2B20000, 0x01000002

# u32 regs_keep(void (*wait)(void)): holds a pattern in s0 to s7, fp and f20
# to f31 across a call of wait, which gives up the CPU.
	.globl	regs_keep
regs_keep:
	frame
	move	$25, $4
	li	$8, 0x01000003
	ctc1	$8, $31
	.irp	n, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	pattern	$8, 0x3C3C0000C3C30000, 40 + \n
	dmtc1	$8, $f\n
	.endr
	.irp	n, 16, 17, 18, 19, 20, 21, 22, 23, 30
	pattern	$\n, 0x3C3C0000C3C30000, \n
	.endr
	jalr	$25
	nop
	move	$2, $0
	.irp	n, 16, 17, 18, 19, 20, 21, 22, 23, 30
	differs	$\n, 0x3C3C0000C3C30000, \n, $8, $2
	.endr
	.irp	n, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	dmfc1	$9, $f\n
	differs	$9, 0x3C3C0000C3C30000, 40 + \n, $8, $2
	.endr
	cfc1	$9, $31
	li	$8, 0x01000003
	beq	$9, $8, 1f
	nop
	addiu	$2, $2, 1
1:
	leave
