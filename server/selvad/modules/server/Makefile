# Copyright (c) 2022-2023 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   conn.o \
	   pubsub.o \
	   selva_send.o \
	   server.o \
	   server_frame.o \
	   server_msg.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -DSELVA_SERVER_MAIN=1
LDFLAGS += -lutil -ljemalloc_selva
TARGET := ../mod_server$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
