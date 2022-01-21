# SPDX-License-Identifier: MIT
include ../../common.mk

RM_INCLUDE_DIR := ../../include

CFLAGS += -fPIC -O2 -Wno-unused-function -fno-strict-aliasing
CFLAGS += -I$(RM_INCLUDE_DIR)
CC := gcc

OBJS := \
	auto_free.o \
	base64.o \
	bitmap.o \
	cstrings.o \
	mempool.o \
	poptop.o \
	queue_r.o \
	strnstrn.o \
	svector.o \
	trx.o
ifeq ($(uname_S),Darwin) # macOS
	OBJS += memrchr.o
endif

all: libutil.a

libutil.a: $(OBJS)
	ar rcs $@ $^
