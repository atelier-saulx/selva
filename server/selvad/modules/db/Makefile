# Copyright (c) 2022-2023 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   db.o \
	   module/aggregate.o \
	   module/alias.o \
	   module/arg_parser.o \
	   module/async_task.o \
	   module/comparator.o \
	   module/dump.o \
	   module/edge/edge.o \
	   module/edge/edge_constraint.o \
	   module/find.o \
	   module/find_index/find_index.o \
	   module/find_index/icb.o \
	   module/find_index/pick_icb.o \
	   module/hierarchy/field_set.o \
	   module/hierarchy/hierarchy.o \
	   module/hierarchy/hierarchy_detached.o \
	   module/hierarchy/hierarchy_inactive.o \
	   module/hierarchy/hierarchy_reply.o \
	   module/hierarchy/traversal.o \
	   module/hierarchy/traversal_order.o \
	   module/hierarchy/types.o \
	   module/inherit/get_field.o \
	   module/inherit/inherit.o \
	   module/inherit/send_field.o \
	   module/inherit/send_field_find.o \
	   module/modify.o \
	   module/resolve.o \
	   module/rpn/rpn.o \
	   module/rpn/rpn_eval.o \
	   module/sdb_purge.o \
	   module/selva_lang.o \
	   module/selva_object/selva_object.o \
	   module/selva_object/selva_object_commands.o \
	   module/selva_object/selva_object_foreach.o \
	   module/selva_set/field_has.o \
	   module/selva_set/fielda_in_fieldb.o \
	   module/selva_set/fielda_in_setb.o \
	   module/selva_set/selva_set.o \
	   module/selva_set/seta_in_fieldb.o \
	   module/selva_set/seta_in_setb.o \
	   module/selva_type.o \
	   module/shared_strings.o \
	   module/subscriptions.o \
	   module/update.o
#	   module/modinfo.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -I./include -include ../../tunables.h
LDFLAGS += -ldeflate -lutil -ljemalloc_selva -lhiredis
TARGET := ../mod_db$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)
