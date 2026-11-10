# The thunks that tools/thunks.py cannot write: each call between the game's
# assembly, which keeps the o32 calls, and our C at o64 that passes a 64-bit
# value. At o32 such a value is in two registers or two stack slots, the high
# half first, and at o64 in one.
	.set noreorder
	.set noat
	.section .text, "ax"

# OSTime osGetTime(void)
	.global osGetTime
osGetTime:
	addiu	$sp, $sp, -48
	sd	$31, 40($sp)
	jal	c_osGetTime
	nop
	ld	$31, 40($sp)
	sll	$3, $2, 0
	dsra32	$2, $2, 0
	jr	$31
	addiu	$sp, $sp, 48

# int osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg)
	.global osSetTimer
osSetTimer:
	addiu	$sp, $sp, -64
	sd	$31, 56($sp)
	dsll32	$5, $6, 0
	dsll32	$7, $7, 0
	dsrl32	$7, $7, 0
	or	$5, $5, $7
	lw	$6, 80($sp)
	lwu	$8, 84($sp)
	dsll32	$6, $6, 0
	or	$6, $6, $8
	lw	$7, 88($sp)
	lw	$8, 92($sp)
	sd	$8, 32($sp)
	jal	c_osSetTimer
	nop
	ld	$31, 56($sp)
	jr	$31
	addiu	$sp, $sp, 64

# void ultra_call(void (*entry)(void *), void *arg): calls a routine that may
# be the game's, so it keeps all 64 bits of each saved register.
	.global ultra_call
ultra_call:
	addiu	$sp, $sp, -112
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
	move	$25, $4
	jalr	$25
	move	$4, $5
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
	addiu	$sp, $sp, 112
