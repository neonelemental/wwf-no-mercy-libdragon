# A console ROM that starts libdragon from the stock IPL3: build/boot.z64.
# It is a flat image at 0x80000400 after the header and IPL3 of ../../baserom.z64.

N64_INST ?= $(abspath ../../build/libdragon/n64)
BUILD_DIR = build
include $(N64_INST)/include/n64.mk

BASEROM = ../../baserom.z64
OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/main.o
# n64.mk's link flags, with boot.ld in place of n64.ld.
BOOT_LDFLAGS = $(subst -Tn64.ld,-Tboot.ld,$(N64_LDFLAGS))

.PHONY: all clean
all: $(BUILD_DIR)/boot.z64

# n64.mk's %.z64 sets the compiler and its flags for each prerequisite. This
# rule takes the place of its recipe, so no n64tool runs.
$(BUILD_DIR)/boot.z64: $(BUILD_DIR)/boot.bin $(BASEROM) ../../tools/n64crc.py
	head -c 4096 $(BASEROM) > $@.tmp
	cat $< >> $@.tmp
	truncate -s '>1052672' $@.tmp
	python3 ../../tools/n64crc.py $@.tmp
	mv $@.tmp $@

# The base ROM is never built: this rule keeps n64.mk's %.z64 from it.
$(BASEROM):
	@echo "$@ is missing: README.md gives the base ROM" >&2; exit 1

$(BUILD_DIR)/boot.bin: $(BUILD_DIR)/boot.elf
	$(N64_OBJCOPY) -O binary $< $@

$(BUILD_DIR)/boot.elf: $(OBJS) boot.ld $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a
	$(N64_CC) $(N64_C_AND_CXX_FLAGS) -o $@ $(OBJS) $(patsubst %,-Wl$(COMMA)%,$(BOOT_LDFLAGS)) \
		-Wl,--start-group -lc -Wl,-ldragon,-ldragonsys -Wl,--end-group -Wl,-Map=$(BUILD_DIR)/boot.map
	$(N64_SIZE) -G $@

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
