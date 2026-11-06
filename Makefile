# Run inside the image from the Dockerfile; README.md gives the command.

.DEFAULT_GOAL := source

include libdragon.mk

SPLIT := build/split

# PAD puts zero bytes at the start of main text, data and bss, and of each
# overlay's text, so that each reference must move with the code.
PAD ?= 0
ifeq ($(PAD),0)
VARIANT ?= source
else
VARIANT ?= pad$(patsubst 0x%,%,$(PAD))
endif
OUT := build/$(VARIANT)

AS      := mips64-elf-as
LD      := mips64-elf-ld
OBJCOPY := mips64-elf-objcopy

ASFLAGS := -EB -march=vr4300 -mabi=32 -G0 -I $(SPLIT)/include

TESTS := $(patsubst %/Makefile,%,$(wildcard tests/*/Makefile))

.PHONY: tests $(TESTS) extract match source check clean
tests: $(TESTS)

$(TESTS): libdragon
	$(MAKE) -C $@

# Splits baserom.z64 into build/split/; splat refuses a dump with another SHA1.
extract: $(SPLIT)/.done
$(SPLIT)/.done: nomercy.yaml symbol_addrs.txt extra_addrs.txt reloc_addrs.txt baserom.z64
	rm -rf $(SPLIT)
	python3 -m splat split nomercy.yaml
	touch $@

match: extract
	$(MAKE) VARIANT=match PAD=0 build/match/nomercy.z64

source: extract
	$(MAKE) $(OUT)/nomercy.z64

check: match
	sha1sum -c nomercy.sha1

ifneq ($(wildcard $(SPLIT)/.done),)
SPLIT_S   := $(shell find $(SPLIT)/asm -name '*.s')
SPLIT_BIN := $(shell find $(SPLIT)/assets -name '*.bin')
SPLIT_O   := $(patsubst %,$(SPLIT)/obj/%.o,$(SPLIT_S) $(SPLIT_BIN))
endif

$(SPLIT)/obj/%.s.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) -o $@ $<

# Through as, so each object has the same ABI flags.
$(SPLIT)/obj/%.bin.o: %.bin
	@mkdir -p $(@D)
	printf '.section .data\n.incbin "%s"\n' $< | $(AS) $(ASFLAGS) -o $@ -

$(OUT)/nomercy.ld: $(SPLIT)/.done tools/link_script.py
	python3 tools/link_script.py $(SPLIT) $(OUT)

$(OUT)/nomercy.elf: $(SPLIT_O) $(OUT)/nomercy.ld
	@rm -f $(OUT)/nomercy.z64
	$(LD) -m elf32ebmip --emit-relocs --defsym PAD=$(PAD) -T $(OUT)/nomercy.ld \
		-T $(OUT)/undefined_funcs.ld -T $(OUT)/undefined_syms.ld \
		-Map $(OUT)/nomercy.map -o $@

# No segment holds the 0xFF fill at the end of the retail image. The match
# keeps the retail CRC; each other variant gets its own.
$(OUT)/nomercy.z64: $(OUT)/nomercy.elf
	$(OBJCOPY) -O binary --gap-fill=0xFF --pad-to=0x2000000 $< $@
ifneq ($(VARIANT),match)
	python3 tools/n64crc.py $@
endif

clean:
	rm -rf build tests/*/build
