# libdragon, built from the submodule into build/libdragon/.
# The sources are copied without .git, so the build never runs git.

LIBDRAGON_SRC := $(CURDIR)/build/libdragon/src-stock
LIBDRAGON_STOCK := $(CURDIR)/build/libdragon/stock
LIBDRAGON_MAKE = $(MAKE) -C $(LIBDRAGON_SRC) N64_INST=$(LIBDRAGON_STOCK)
# Each file of the submodule, so a change to it builds libdragon again.
LIBDRAGON_FILES := $(shell find libdragon -name .git -prune -o -type f -print 2>/dev/null)

.PHONY: libdragon
libdragon: $(LIBDRAGON_STOCK)/.built

# The order of libdragon's build.sh.
$(LIBDRAGON_STOCK)/.built: libdragon.mk $(LIBDRAGON_FILES)
	@test -f libdragon/Makefile || { echo "libdragon/ is empty: run git submodule update --init" >&2; exit 1; }
	rm -rf $(LIBDRAGON_SRC) $(LIBDRAGON_STOCK)
	mkdir -p $(LIBDRAGON_SRC)
	tar -C libdragon --exclude=.git -cf - . | tar -C $(LIBDRAGON_SRC) -xf -
	$(LIBDRAGON_MAKE) install-mk
	$(LIBDRAGON_MAKE) libdragon tools
	$(LIBDRAGON_MAKE) install tools-install
	touch $@
