# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   selva_proto.o \
	   server.o
DEP := $(OBJ:%.o=%.d)
LDFLAGS += -lutil
TARGET := ../server$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
