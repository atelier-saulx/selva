LIBDEFLATE_VER ?= 1.14

ifeq ($(uname_S),Linux)
	TARGET := libdeflate.so
endif
ifeq ($(uname_S),Darwin)
	TARGET := libdeflate.dylib
endif
ifndef TARGET
$(error TARGET is not set)
endif

deflate:
	$(MAKE) CFLAGS=-fPIC DISABLE_GZIP=1 DISABLE_ZLIB=1 -C ./libdeflate-$(LIBDEFLATE_VER) $(TARGET)
	rsync --checksum "./libdeflate-$(LIBDEFLATE_VER)/libdeflate.h" "../../include/libdeflate.h"
	cp -P libdeflate-$(LIBDEFLATE_VER)/libdeflate.so "../"
	cp -P libdeflate-$(LIBDEFLATE_VER)/libdeflate.so.0 "../"

clean: 
	$(MAKE) -C ./libdeflate-$(LIBDEFLATE_VER) clean

.PHONY: clean deflate
