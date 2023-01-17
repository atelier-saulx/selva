# Copyright (c) 2023 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   replication.o \
	   ring_buffer.o
DEP := $(OBJ:%.o=%.d)
LDFLAGS += -lutil -ljemalloc_selva
TARGET := ../mod_replication$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
