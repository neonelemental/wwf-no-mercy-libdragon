# libdragon, built from the submodule into build/libdragon/.
# The sources are copied without .git, so the build never runs git.
# stock is libdragon as it is. o32 has each patches/libdragon/*.patch applied,
# for code at o32 with FR 0.

LIBDRAGON_SRC := $(CURDIR)/build/libdragon/src-stock
LIBDRAGON_STOCK := $(CURDIR)/build/libdragon/stock
LIBDRAGON_MAKE = $(MAKE) -C $(LIBDRAGON_SRC) N64_INST=$(LIBDRAGON_STOCK)
LIBDRAGON_O32_SRC := $(CURDIR)/build/libdragon/src-o32
LIBDRAGON_O32 := $(CURDIR)/build/libdragon/o32
LIBDRAGON_PATCHES := $(sort $(wildcard patches/libdragon/*.patch))
# Each file of the submodule, so a change to it builds libdragon again.
LIBDRAGON_FILES := $(shell find libdragon -name .git -prune -o -type f -print 2>/dev/null)

.PHONY: libdragon
libdragon: $(LIBDRAGON_STOCK)/.built $(LIBDRAGON_O32)/.built

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

# n64.mk finds its tools in $(N64_INST)/bin, so o32 takes those of stock.
$(LIBDRAGON_O32)/.built: $(LIBDRAGON_STOCK)/.built $(LIBDRAGON_PATCHES)
	rm -rf $(LIBDRAGON_O32_SRC) $(LIBDRAGON_O32)
	mkdir -p $(LIBDRAGON_O32_SRC) $(LIBDRAGON_O32)
	tar -C libdragon --exclude=.git -cf - . | tar -C $(LIBDRAGON_O32_SRC) -xf -
	set -e; for p in $(LIBDRAGON_PATCHES); do patch -d $(LIBDRAGON_O32_SRC) -p1 < $$p; done
	ln -s ../stock/bin $(LIBDRAGON_O32)/bin
	$(MAKE) -C $(LIBDRAGON_O32_SRC) N64_INST=$(LIBDRAGON_O32) N64_GCCPREFIX=/opt/libdragon install
	touch $@
