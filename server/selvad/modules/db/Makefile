# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   db.o
DEP := $(OBJ:%.o=%.d)
LDFLAGS += -lutil
TARGET := ../db$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
