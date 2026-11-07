# Free space at the end of main text, for code that a patch of the image
# writes. No code of the game reaches it.
	.section .text, "ax"

	.global patch_space
	.type patch_space, @object
patch_space:
	.space 0x6C
	.size patch_space, . - patch_space

	# Main data starts after this object. 0x80 keeps it at its retail place
	# modulo 0x40, as a pad does.
	.org 0x80
