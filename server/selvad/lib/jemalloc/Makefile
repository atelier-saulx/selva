JEMALLOC_VER := 5.3.0
JEMALLOC_CONFIG := --with-version=VERSION \
				   --with-lg-quantum=3 \
				   --disable-cxx \
				   --enable-xmalloc \
				   --disable-initial-exec-tls \
				   --with-jemalloc-prefix=selva_ \
				   --with-private-namespace=selva_ \
				   --with-install-suffix=_selva \
				   --with-malloc-conf=xmalloc:true
JEMALLOC_CFLAGS := -std=gnu99 -fPIC -Wall -Wno-missing-braces -pipe -g3 -O3 -funroll-loops
JEMALLOC_LDFLAGS := $(LDFLAGS)
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
	SELVA_ARCH := linux_x64
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
	SELVA_ARCH := darwin_x64
    JEMALLOC_LIBDIR := "@executable_path"
endif

.make-prerequisites:
	@touch jemalloc-$(JEMALLOC_VER)

all: .make-prerequisites jemalloc-$(JEMALLOC_VER)/config.status
	cd jemalloc-$(JEMALLOC_VER) && $(MAKE) CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)" LIBDIR="$(JEMALLOC_LIBDIR)" build_lib_shared
#	cp -P jemalloc-$(JEMALLOC_VER)/lib/* "../binaries/$(SELVA_ARCH)/"
#	cp jemalloc-$(JEMALLOC_VER)/include/jemalloc/jemalloc_selva.h "../selva/include/jemalloc_$(SELVA_ARCH).h"

jemalloc-$(JEMALLOC_VER)/config.status:
	cd jemalloc-$(JEMALLOC_VER) && ./configure $(JEMALLOC_CONFIG) CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)" LIBDIR="$(JEMALLOC_LIBDIR)"

clean:
	cd jemalloc-$(JEMALLOC_VER) && $(MAKE) clean
