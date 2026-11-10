# A console ROM that plays samples of the base ROM on libdragon's mixer and
# times each poll: build/mixer.z64. It starts as tests/boot does.

N64_INST ?= $(abspath ../../build/libdragon/n64)
BUILD_DIR = build
LIBDRAGON_PREVIEW = 2
include $(N64_INST)/include/n64.mk

BASEROM = ../../baserom.z64
OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/main.o $(BUILD_DIR)/blob.o
# n64.mk's link flags, with boot.ld in place of n64.ld.
BOOT_LDFLAGS = $(subst -Tn64.ld,-Tboot.ld,$(N64_LDFLAGS))

.PHONY: all clean
all: $(BUILD_DIR)/mixer.z64

$(BUILD_DIR)/mixer.z64: $(BUILD_DIR)/mixer.bin $(BASEROM) ../../tools/n64crc.py
	head -c 4096 $(BASEROM) > $@.tmp
	cat $< >> $@.tmp
	truncate -s '>1052672' $@.tmp
	python3 ../../tools/n64crc.py $@.tmp
	mv $@.tmp $@

$(BASEROM):
	@echo "$@ is missing: README.md gives the base ROM" >&2; exit 1

# The samples and their codebooks, read from the base ROM at build time.
$(BUILD_DIR)/samples.bin $(BUILD_DIR)/samples.h: samples.py $(BASEROM)
	@mkdir -p $(BUILD_DIR)
	python3 samples.py $(BASEROM) $(BUILD_DIR)/samples

$(BUILD_DIR)/blob.o: $(BUILD_DIR)/samples.bin
$(BUILD_DIR)/main.o: $(BUILD_DIR)/samples.h

$(BUILD_DIR)/mixer.bin: $(BUILD_DIR)/mixer.elf
	$(N64_OBJCOPY) -O binary $< $@

$(BUILD_DIR)/mixer.elf: $(OBJS) boot.ld $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a
	$(N64_CC) $(N64_C_AND_CXX_FLAGS) -o $@ $(OBJS) $(patsubst %,-Wl$(COMMA)%,$(BOOT_LDFLAGS)) \
		-Wl,--wrap=rsp_load_data -Wl,--start-group -lc -Wl,-ldragon,-ldragonsys -Wl,--end-group -Wl,-Map=$(BUILD_DIR)/mixer.map
	$(N64_SIZE) -G $@

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/*.d)
