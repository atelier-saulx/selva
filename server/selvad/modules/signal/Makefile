# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   main.o \
	   trace.o

DEP := $(OBJ:%.o=%.d)

TARGET := ../libsignal$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
