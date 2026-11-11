# Run inside the image from the Dockerfile; README.md gives the command.

.DEFAULT_GOAL := source

include libdragon.mk

SPLIT := build/split

# PAD puts zero bytes at the start of main text, data and bss, and of each
# overlay's text, so that each reference must move with the code.
PAD ?= 0
# CHECK names lifted routines, or each of them with `all`: build/check/ then
# keeps their assembly and runs it beside our C.
CHECK ?=
ifneq ($(CHECK),)
VARIANT ?= check
else ifeq ($(PAD),0)
VARIANT ?= source
else
VARIANT ?= pad$(patsubst 0x%,%,$(PAD))
endif
OUT := build/$(VARIANT)

AS      := mips64-elf-as
CC      := mips64-elf-gcc
LD      := mips64-elf-ld
NM      := mips64-elf-nm
OBJCOPY := mips64-elf-objcopy

# The split holds instructions and no macro, so the o64 mark of the assembler
# changes no byte of it. Our C is at o64, as libdragon is.
ASFLAGS := -EB -march=vr4300 -mabi=o64 -G0 -I $(SPLIT)/include
CFLAGS  := -march=vr4300 -mtune=vr4300 -mfix4300 -G0 -O2 -mno-abicalls -fno-pic \
	-ffreestanding -fno-common -DN64 -DLIBDRAGON_PREVIEW=2 -I src/include \
	-I $(LIBDRAGON)/mips64-elf/include/newlib_overrides -I $(LIBDRAGON)/mips64-elf/include -include ktls.h

TESTS := $(patsubst %/Makefile,%,$(wildcard tests/*/Makefile))

.PHONY: tests $(TESTS) harness play profile extract match source check clean FORCE
tests: $(TESTS)

$(TESTS): libdragon
	$(MAKE) -C $@

# The program of tests/tools that runs a ROM in the image's emulator.
HARNESS := build/harness/release/harness
PLAY    := $(OUT)/play
SCRIPTS := tests/tools/scripts

harness:
	cargo build --release --manifest-path tests/tools/Cargo.toml --target-dir build/harness

# Plays the ROM of the variant three times on one cartridge save: a boot that
# formats it, a match that must write to it, and a boot that must take the
# save of the match as it is.
play: harness
	rm -rf $(PLAY)
	$(HARNESS) $(OUT)/nomercy.z64 $(SCRIPTS)/menu.txt $(PLAY) $(PLAY)/save
	cp -r $(PLAY)/save $(PLAY)/formatted
	$(HARNESS) $(OUT)/nomercy.z64 $(SCRIPTS)/match.txt $(PLAY) $(PLAY)/save
	if diff -rq $(PLAY)/formatted $(PLAY)/save > /dev/null; then echo "the match wrote no save" >&2; exit 1; fi
	cp -r $(PLAY)/save $(PLAY)/played
	$(HARNESS) $(OUT)/nomercy.z64 $(SCRIPTS)/menu.txt $(PLAY) $(PLAY)/save
	diff -rq $(PLAY)/played $(PLAY)/save || { echo "the boot changed the save of the match" >&2; exit 1; }

# Plays 25 seconds of a match on the ROM of the variant and writes what each
# routine costs in a game frame, and which routines share lines of the
# instruction cache. SCRIPT=tag plays a tag match. The game's idle thread
# loops in IDLE.
PROFILE := $(OUT)/profile
SCRIPT  ?= long
IDLE    := func_800004D0

profile: harness
	rm -rf $(PROFILE)
	$(HARNESS) $(OUT)/nomercy.z64 $(SCRIPTS)/$(SCRIPT).txt $(PROFILE)
	python3 tools/profile.py symbol_addrs.txt $(OUT)/nomercy.elf $(PROFILE)/profile.txt $(IDLE) > $(PROFILE)/table.txt
	python3 tools/alias.py symbol_addrs.txt $(OUT)/nomercy.elf $(PROFILE)/profile.txt > $(PROFILE)/shared.txt
	head -n 24 $(PROFILE)/table.txt

# Splits baserom.z64 into build/split/; splat refuses a dump with another SHA1.
extract: $(SPLIT)/.done
$(SPLIT)/.done: nomercy.yaml symbol_addrs.txt extra_addrs.txt reloc_addrs.txt baserom.z64
	rm -rf $(SPLIT)
	python3 -m splat split nomercy.yaml
	touch $@

match: extract
	$(MAKE) VARIANT=match PAD=0 build/match/nomercy.z64

source: extract libdragon
	$(MAKE) $(OUT)/split/.done
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

ifeq ($(VARIANT),match)

$(OUT)/nomercy.ld: $(SPLIT)/.done tools/link_script.py
	python3 tools/link_script.py $(SPLIT) $(OUT)

$(OUT)/nomercy.elf: $(SPLIT_O) $(OUT)/nomercy.ld
	@rm -f $(OUT)/nomercy.z64
	$(LD) -m elf32ebmip --emit-relocs --defsym PAD=$(PAD) -T $(OUT)/nomercy.ld \
		-T $(OUT)/undefined_funcs.ld -T $(OUT)/undefined_syms.ld \
		-Map $(OUT)/nomercy.map -o $@

# No segment holds the 0xFF fill at the end of the retail image, and the match
# keeps the retail CRC.
$(OUT)/nomercy.z64: $(OUT)/nomercy.elf
	$(OBJCOPY) -O binary --gap-fill=0xFF --pad-to=0x2000000 $< $@

else

# Each other variant assembles the split with the labels of replace/*.txt and
# lift/*.txt cut, and links our code and libdragon after it (dragon.ld). A
# directory src/dragon/PART/ joins only with its list replace/PART.txt.
REPLACE := $(wildcard replace/*.txt)
LIFT    := $(wildcard lift/*.txt)
comma   := ,
PARTS   := $(basename $(notdir $(REPLACE)))
SRC     := $(sort $(filter-out src/dragon/%,$(shell find src -name '*.c' -o -name '*.S')) \
	$(foreach p,$(PARTS),$(shell find src/dragon/$(p) -name '*.c' -o -name '*.S' 2>/dev/null)))
SRC_O   := $(patsubst src/%,$(OUT)/obj/%,$(addsuffix .o,$(basename $(SRC))))
CUT_O   := $(patsubst $(SPLIT)/asm/%,$(OUT)/split/obj/asm/%.o,$(filter $(SPLIT)/asm/%,$(SPLIT_S)))
# Our C goes into the link through tools/thunks.py, and our assembly as it is.
# dragon.ld gives the place of each object in the link.
C_O     := $(patsubst src/%.c,$(OUT)/obj/%.o,$(filter %.c,$(SRC)))
S_O     := $(patsubst src/%.S,$(OUT)/obj/%.o,$(filter %.S,$(SRC)))
THUNK_O := $(patsubst $(OUT)/obj/%,$(OUT)/thunk/%,$(C_O))
OURS_O  := $(S_O) $(THUNK_O) $(OUT)/thunk/thunks.o
LINK_O  := $(CUT_O) $(filter-out $(SPLIT)/obj/$(SPLIT)/asm/%,$(SPLIT_O)) $(OURS_O)

LIBC    := $(shell $(CC) $(CFLAGS) -print-file-name=libc.a)
LIBM    := $(shell $(CC) $(CFLAGS) -print-file-name=libm.a)
LIBGCC  := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LIBS    := $(addprefix $(LIBDRAGON)/mips64-elf/lib/,libdragon.a libdragonsys.a) $(LIBC) $(LIBM) $(LIBGCC)

# A list of the inputs, written when the set of files changes, so that a
# removed source file or replace list also gives a new cut and a new link.
$(OUT)/inputs.txt: FORCE
	@mkdir -p $(@D)
	@echo '$(REPLACE) $(LIFT) $(SRC) $(CHECK)' | cmp -s - $@ || echo '$(REPLACE) $(LIFT) $(SRC) $(CHECK)' > $@

$(OUT)/split/.done: $(SPLIT)/.done tools/cut.py double_addrs.txt $(REPLACE) $(LIFT) $(OUT)/inputs.txt
	python3 tools/cut.py $(SPLIT) $(OUT)/split $(REPLACE) --lift $(LIFT) --check $(subst $(comma), ,$(CHECK))
	touch $@

$(OUT)/split/asm/%.s: $(OUT)/split/.done ;

$(OUT)/split/obj/asm/%.s.o: $(OUT)/split/asm/%.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) -o $@ $<

$(OUT)/obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(OUT)/obj/%.o: src/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(SRC_O:.o=.d)

$(OUT)/thunk/.done: $(C_O) $(S_O) $(CUT_O) tools/thunks.py $(OUT)/split/.done $(OUT)/inputs.txt
	rm -rf $(OUT)/thunk
	python3 tools/thunks.py $(OUT) $(NM) $(OBJCOPY) --split $(CUT_O) --c $(C_O) --hand $(S_O)
	touch $@

$(OUT)/thunk/thunks.S: $(OUT)/thunk/.done ;

$(OUT)/thunk/thunks.o: $(OUT)/thunk/thunks.S
	$(CC) $(CFLAGS) -c -o $@ $<

# An object of our C and its thunks, as one object. Two objects can each hold
# a thunk into the same routine of the assembly, so its name stays inside.
$(THUNK_O): $(OUT)/thunk/%.o: $(OUT)/thunk/.done
	$(CC) $(CFLAGS) -c -o $(@:.o=.t.o) $(@:.o=.S)
	$(LD) -m elf32ebmip -r -o $@ $(@:.o=.c.o) $(@:.o=.t.o)
	$(OBJCOPY) -w -L 'asm_*' $@

$(OUT)/nomercy.ld: $(SPLIT)/.done tools/link_script.py $(REPLACE) $(LIFT) $(OUT)/inputs.txt
	python3 tools/link_script.py $(SPLIT) $(OUT) --source $(REPLACE) $(LIFT)

# The link gives the dragon segment a rom past the files when they reach it,
# so that this check can give both numbers.
$(OUT)/nomercy.elf: $(LINK_O) $(OUT)/nomercy.ld dragon.ld $(OUT)/split/.done $(OUT)/inputs.txt $(LIBS)
	@rm -f $(OUT)/nomercy.z64
	$(LD) -m elf32ebmip --emit-relocs --gc-sections --defsym PAD=$(PAD) @$(OUT)/defsyms.txt \
		-T $(OUT)/nomercy.ld -T dragon.ld -T $(OUT)/undefined_funcs.ld -T $(OUT)/undefined_syms.ld \
		$(OURS_O) --start-group $(LIBS) --end-group -Map $(OUT)/nomercy.map -o $@.tmp
	end=$$($(NM) $@.tmp | awk '$$3 == "files_ROM_END" { print $$1 }'); \
	if [ $$((0x$$end)) -gt $$((0x2000000)) ]; then \
		echo "the files end at rom 0x$$end, past the dragon segment at rom 0x2000000" >&2; exit 1; fi
	mv $@.tmp $@

# The 0xFF fill runs from the end of the files to the dragon segment, and the
# image ends at the next MB after that segment.
$(OUT)/nomercy.z64: $(OUT)/nomercy.elf
	end=$$($(NM) $< | awk '$$3 == "dragon_ROM_END" { print $$1 }'); \
	$(OBJCOPY) -O binary --gap-fill=0xFF --pad-to=$$(( (0x$$end + 0xFFFFF) / 0x100000 * 0x100000 )) $< $@
	python3 tools/n64crc.py $@

endif

clean:
	rm -rf build tests/*/build
