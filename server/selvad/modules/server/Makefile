# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   conn.o \
	   server.o \
	   server_frame.o \
	   server_msg.o \
	   selva_send.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -DSELVA_SERVER_MAIN=1
LDFLAGS += -lutil -ljemalloc_selva
TARGET := ../mod_server$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
