# Copyright (c) 2023 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   main.o \
	   origin/replica_thread.o \
	   origin/replication.o \
	   origin/ring_buffer.o \
	   replica/replication.o \
	   selva_thread.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -DSELVA_REPLICATION_MAIN=1
LDFLAGS += -lutil -ljemalloc_selva
TARGET := ../mod_replication$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
