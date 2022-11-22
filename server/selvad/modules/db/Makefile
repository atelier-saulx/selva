# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   db.o \
	   module/arg_parser.o \
	   module/config.o \
	   module/find.o \
	   module/hierarchy/hierarchy.o \
	   module/hierarchy/hierarchy_reply.o \
	   module/hierarchy/traversal.o \
	   module/hierarchy/traversal_order.o \
	   module/hierarchy/types.o \
	   module/rpn/rpn.o \
	   module/rpn/rpn_eval.o \
	   module/selva_lang.o \
	   module/selva_object/selva_object.o \
	   module/selva_object/selva_object_foreach.o \
	   module/selva_set/field_has.o \
	   module/selva_set/fielda_in_fieldb.o \
	   module/selva_set/fielda_in_setb.o \
	   module/selva_set/selva_set.o \
	   module/selva_set/seta_in_fieldb.o \
	   module/selva_set/seta_in_setb.o \
	   module/selva_type.o \
	   module/shared_strings.o
#	   module/selva_object/selva_object_commands.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -I./include -include ../../tunables.h
LDFLAGS += -lutil
TARGET := ../db$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
