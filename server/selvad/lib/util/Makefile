# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT
include ../../common.mk

CFLAGS += -fPIC -O2 -Wno-unused-function -fno-strict-aliasing -I../../include
# To remove warnings about asserts
CFLAGS += -Wno-unused-value
LDFLAGS += -L../
LDLIBS += -ljemalloc_selva

OBJ := \
	auto_free.o \
	base64.o \
	bitmap.o \
	cstrings.o \
	mempool.o \
	poptop.o \
	queue_r.o \
	svector.o \
	trx.o
ifeq ($(uname_S),Darwin) # macOS
	OBJ += memrchr.o
endif

ifndef LIB_SUFFIX
$(error LIB_SUFFIX is not set)
endif
TARGET := ../llibutil$(LIB_SUFFIX)

DEP := $(OBJ:%.o=%.d)

$(TARGET): $(OBJ)
	$(CC) -o $@ -shared -fPIC -g $^ $(LDFLAGS) $(LDLIBS)

-include $(DEP)
