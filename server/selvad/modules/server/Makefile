# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   server.o \
	   server_frame.o \
	   server_send.o
DEP := $(OBJ:%.o=%.d)
LDFLAGS += -lutil
TARGET := ../server$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
