# Run inside the image from the Dockerfile; README.md gives the command.

.DEFAULT_GOAL := tests

include libdragon.mk

TESTS := $(patsubst %/Makefile,%,$(wildcard tests/*/Makefile))

.PHONY: tests $(TESTS) clean
tests: $(TESTS)

$(TESTS): libdragon
	$(MAKE) -C $@

clean:
	rm -rf build tests/*/build
