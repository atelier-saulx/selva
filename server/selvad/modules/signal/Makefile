# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   main.o \
	   trace.o

DEP := $(OBJ:%.o=%.d)
TARGET := ../mod_signal$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
