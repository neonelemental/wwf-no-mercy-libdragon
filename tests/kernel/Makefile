# A console ROM that runs libultra's thread, queue and event calls of
# src/dragon/kernel on libdragon: build/kernel.z64. It starts from the
# stock IPL3 with the boot code and linker script of tests/boot.

N64_INST ?= $(abspath ../../build/libdragon/n64)
BUILD_DIR = build
include $(N64_INST)/include/n64.mk

BASEROM = ../../baserom.z64
BOOT_DIR = ../boot
KERNEL_DIR = ../../src/dragon/kernel
KERNEL_OBJS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_DIR)/kernel/%.o,$(wildcard $(KERNEL_DIR)/*.c))
OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/main.o $(BUILD_DIR)/regs.o $(KERNEL_OBJS)
CFLAGS += -I../../src/include
# n64.mk's link flags, with boot.ld in place of n64.ld.
BOOT_LDFLAGS = $(subst -Tn64.ld,-T$(BOOT_DIR)/boot.ld,$(N64_LDFLAGS))

.PHONY: all clean
all: $(BUILD_DIR)/kernel.z64

# n64.mk's %.z64 sets the compiler and its flags for each prerequisite. This
# rule takes the place of its recipe, so no n64tool runs.
$(BUILD_DIR)/kernel.z64: $(BUILD_DIR)/kernel.bin $(BASEROM) ../../tools/n64crc.py
	head -c 4096 $(BASEROM) > $@.tmp
	cat $< >> $@.tmp
	truncate -s '>1052672' $@.tmp
	python3 ../../tools/n64crc.py $@.tmp
	mv $@.tmp $@

# The base ROM is never built: this rule keeps n64.mk's %.z64 from it.
$(BASEROM):
	@echo "$@ is missing: README.md gives the base ROM" >&2; exit 1

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(N64_OBJCOPY) -O binary $< $@

$(BUILD_DIR)/boot.o: $(BOOT_DIR)/boot.S
	@mkdir -p $(dir $@)
	$(CC) -c $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -MMD -o $@ $<

$(BUILD_DIR)/kernel.elf: $(OBJS) $(BOOT_DIR)/boot.ld $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a
	$(N64_CC) $(N64_C_AND_CXX_FLAGS) -o $@ $(OBJS) $(patsubst %,-Wl$(COMMA)%,$(BOOT_LDFLAGS)) \
		-Wl,--start-group -lc -Wl,-ldragon,-ldragonsys -Wl,--end-group -Wl,-Map=$(BUILD_DIR)/kernel.map
	$(N64_SIZE) -G $@

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d $(BUILD_DIR)/kernel/*.d)
