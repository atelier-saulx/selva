# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT
include ../../common.mk

CFLAGS += -fPIC -O2 -Wno-unused-function -fno-strict-aliasing -I../../include
# To remove warnings about asserts
CFLAGS += -Wno-unused-value
LDFLAGS += -g -L../

OBJ := \
	auto_free.o \
	base64.o \
	bitmap.o \
	crc32c/crc32c.o \
	cstrings.o \
	ctime.o \
	eztrie.o \
	finalizer.o \
	ida.o \
	mempool.o \
	poptop.o \
	queue_r.o \
	selva_error.o \
	selva_proto.o \
	selva_string.o \
	svector.o \
	timestamp.o \
	trx.o

ifeq ($(uname_S),Linux)
	LDFLAGS += -shared -fPIC
endif
ifeq ($(uname_S),Darwin)
	OBJ += memrchr.o
	LDFLAGS += -shared -Wl,-install_name,@rpath/libutil$(LIB_SUFFIX)
	LDFLAGS += -ljemalloc_selva
endif

ifndef LIB_SUFFIX
$(error LIB_SUFFIX is not set)
endif
TARGET := ../libutil$(LIB_SUFFIX)

DEP := $(OBJ:%.o=%.d)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
