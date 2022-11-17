# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   db.o \
	   module/arg_parser.o \
	   module/selva_lang.o \
	   module/selva_object/selva_object.o \
	   module/selva_object/selva_object_commands.o \
	   module/selva_object/selva_object_foreach.o \
	   module/shared_strings.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -I./include -include ../../tunables.h
LDFLAGS += -lutil
TARGET := ../db$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
