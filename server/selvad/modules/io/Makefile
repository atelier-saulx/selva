# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   io.o
LDFLAGS += -lutil
CFLAGS += -DSELVA_IO_MAIN=1
TARGET := ../io$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
