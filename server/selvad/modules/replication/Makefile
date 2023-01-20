# Copyright (c) 2023 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   main.o \
	   origin/replica_thread.o \
	   origin/replication.o \
	   origin/ring_buffer.o
DEP := $(OBJ:%.o=%.d)
LDFLAGS += -lutil -ljemalloc_selva
TARGET := ../mod_replication$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
