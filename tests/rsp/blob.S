# The part of the base ROM that the Makefile copies to build/rsp.blob.
    .section .data
    .balign 16
    .global rsp_blob
rsp_blob:
    .incbin "build/rsp.blob"
