JEMALLOC_VER := 5.3.0
JEMALLOC_CONFIG := --with-version=VERSION \
				   --with-lg-quantum=3 \
				   --disable-cxx \
				   --enable-xmalloc \
				   --disable-initial-exec-tls \
				   --with-jemalloc-prefix=selva_ \
				   --with-private-namespace=selva_ \
				   --with-malloc-conf=xmalloc:true
JEMALLOC_CFLAGS := -std=gnu99 -fPIC -Wall -Wno-missing-braces -pipe -g3 -O3 -funroll-loops
JEMALLOC_LDFLAGS := $(LDFLAGS)

.make-prerequisites:
	@touch jemalloc-$(JEMALLOC_VER)

all: .make-prerequisites jemalloc-$(JEMALLOC_VER)/config.status
	cd jemalloc-$(JEMALLOC_VER) && $(MAKE) CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)" build_lib_shared
	cp -P jemalloc-$(JEMALLOC_VER)/lib/* ../binaries/linux_x64/
	cp jemalloc-$(JEMALLOC_VER)/include/jemalloc/jemalloc.h ../selva/include/jemalloc.h

jemalloc-$(JEMALLOC_VER)/config.status:
	cd jemalloc-$(JEMALLOC_VER) && ./configure $(JEMALLOC_CONFIG) CFLAGS="$(JEMALLOC_CFLAGS)" LDFLAGS="$(JEMALLOC_LDFLAGS)"

clean:
	cd jemalloc-$(JEMALLOC_VER) && $(MAKE) clean
