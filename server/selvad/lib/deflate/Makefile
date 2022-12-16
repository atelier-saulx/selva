# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT
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
ifeq ($(uname_S),Linux)
	cp -P libdeflate-$(LIBDEFLATE_VER)/libdeflate.so "../"
	cp -P libdeflate-$(LIBDEFLATE_VER)/libdeflate.so.0 "../"
endif
ifeq ($(uname_S),Darwin)
	install_name_tool -id @rpath/libdeflate.dylib libdeflate-$(LIBDEFLATE_VER)/libdeflate.dylib
	install_name_tool -id @rpath/libdeflate.0.dylib libdeflate-$(LIBDEFLATE_VER)/libdeflate.0.dylib
	cp -P libdeflate-$(LIBDEFLATE_VER)/libdeflate.dylib "../"
	cp -P libdeflate-$(LIBDEFLATE_VER)/libdeflate.0.dylib "../"
endif

clean: 
	$(MAKE) -C ./libdeflate-$(LIBDEFLATE_VER) clean

.PHONY: clean deflate
