# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT
include ../../common.mk

RM_INCLUDE_DIR := ../../include ../jemalloc

CFLAGS += -fPIC -O2 -Wno-unused-function -fno-strict-aliasing
CFLAGS += $(patsubst %,-I%,$(RM_INCLUDE_DIR))
CC := gcc

OBJS := \
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
	OBJS += memrchr.o
endif

all: libutil.a

libutil.a: $(OBJS)
	ar rcs $@ $^
