# A console ROM that proves the libdragon build: build/hello.z64.

N64_INST ?= $(abspath ../../build/libdragon/n64)
BUILD_DIR = build
include $(N64_INST)/include/n64.mk

.PHONY: all clean

# n64.mk writes the ROM beside this Makefile. A rule with build/hello.z64 as
# its target would also match n64.mk's %.z64 and add its link flags twice, so
# a second make builds the ROM and this one moves it under build/.
all:
	$(MAKE) hello.z64
	mv hello.z64 $(BUILD_DIR)/hello.z64

$(BUILD_DIR)/hello.elf: $(BUILD_DIR)/main.o
hello.z64: N64_ROM_TITLE = "hello"

clean:
	rm -rf $(BUILD_DIR) hello.z64

-include $(wildcard $(BUILD_DIR)/*.d)
