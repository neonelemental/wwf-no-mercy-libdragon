# Run inside the image from the Dockerfile; README.md gives the command.

.DEFAULT_GOAL := match

include libdragon.mk

SPLIT := build/split
MATCH := build/match

AS      := mips64-elf-as
LD      := mips64-elf-ld
OBJCOPY := mips64-elf-objcopy

ASFLAGS := -EB -march=vr4300 -mabi=32 -G0 -I $(SPLIT)/include

TESTS := $(patsubst %/Makefile,%,$(wildcard tests/*/Makefile))

.PHONY: tests $(TESTS) extract match check clean
tests: $(TESTS)

$(TESTS): libdragon
	$(MAKE) -C $@

# Splits baserom.z64 into build/split/; splat refuses a dump with another SHA1.
extract: $(SPLIT)/.done
$(SPLIT)/.done: nomercy.yaml symbol_addrs.txt baserom.z64
	rm -rf $(SPLIT)
	python3 -m splat split nomercy.yaml
	touch $@

match: extract
	$(MAKE) $(MATCH)/nomercy.z64

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

$(MATCH)/nomercy.elf: $(SPLIT_O) $(SPLIT)/nomercy.ld
	@mkdir -p $(@D)
	@rm -f $(MATCH)/nomercy.z64
	$(LD) -m elf32ebmip --emit-relocs -T $(SPLIT)/nomercy.ld \
		-T $(SPLIT)/undefined_funcs_auto.txt -T $(SPLIT)/undefined_syms_auto.txt \
		-Map $(MATCH)/nomercy.map -o $@

# No segment holds the 0xFF fill at the end of the retail image.
$(MATCH)/nomercy.z64: $(MATCH)/nomercy.elf
	$(OBJCOPY) -O binary --gap-fill=0xFF --pad-to=0x2000000 $< $@

clean:
	rm -rf build tests/*/build
