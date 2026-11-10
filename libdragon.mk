# libdragon, built from the submodule into build/libdragon/n64, at its own
# flags, with each patches/libdragon/*.patch applied.
# The sources are copied without .git, so the build never runs git.

LIBDRAGON_SRC := $(CURDIR)/build/libdragon/src
LIBDRAGON := $(CURDIR)/build/libdragon/n64
LIBDRAGON_MAKE = $(MAKE) -C $(LIBDRAGON_SRC) N64_INST=$(LIBDRAGON)
LIBDRAGON_PATCHES := $(sort $(wildcard patches/libdragon/*.patch))
# Each file of the submodule, so a change to it builds libdragon again.
LIBDRAGON_FILES := $(shell find libdragon -name .git -prune -o -type f -print 2>/dev/null)

.PHONY: libdragon
libdragon: $(LIBDRAGON)/.built

# The order of libdragon's build.sh.
$(LIBDRAGON)/.built: libdragon.mk $(LIBDRAGON_PATCHES) $(LIBDRAGON_FILES)
	@test -f libdragon/Makefile || { echo "libdragon/ is empty: run git submodule update --init" >&2; exit 1; }
	rm -rf $(LIBDRAGON_SRC) $(LIBDRAGON)
	mkdir -p $(LIBDRAGON_SRC)
	tar -C libdragon --exclude=.git -cf - . | tar -C $(LIBDRAGON_SRC) -xf -
	set -e; for p in $(LIBDRAGON_PATCHES); do patch -d $(LIBDRAGON_SRC) -p1 < $$p; done
	$(LIBDRAGON_MAKE) install-mk
	$(LIBDRAGON_MAKE) libdragon tools
	$(LIBDRAGON_MAKE) install tools-install
	touch $@
