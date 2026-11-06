# The samples that the Makefile copies to build/samples.bin.
    .section .data
    .balign 16
    .global sample_blob
sample_blob:
    .incbin "build/samples.bin"
