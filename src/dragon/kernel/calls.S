# The thunks of the three calls that a frame makes most, from the game's
# assembly at o32 to our C at o64. Each call passes at most four words, all in
# registers, so its thunk makes the o64 frame and copies nothing, where a
# thunk of tools/thunks.py copies four stack slots.
	.set noreorder
	.set noat
	.section .text, "ax"

	.macro	call name
	.global	\name
\name:
	addiu	$sp, $sp, -48
	sd	$31, 40($sp)
	jal	c_\name
	nop
	ld	$31, 40($sp)
	jr	$31
	addiu	$sp, $sp, 48
	.endm

	call	osRecvMesg
	call	osSendMesg
	call	osSetIntMask
