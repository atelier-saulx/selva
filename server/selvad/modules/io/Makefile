# Copyright (c) 2022-2023 SAULX
# SPDX-License-Identifier: MIT

SELVA_DB_VERSION := $(shell sh -c 'git rev-parse --verify HEAD')
OBJ := \
	   io.o \
	   myreadlink.o \
	   sdb.o \
	   version.o
DEP := $(OBJ:%.o=%.d)
CFLAGS += -DSELVA_IO_MAIN=1 -DSELVA_IO_TYPE
LDFLAGS += -lutil -lsha3iuf -ljemalloc_selva
TARGET := ../mod_io$(MOD_SUFFIX)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

-include $(DEP)

version.c: FORCE
ifeq ($(SELVA_IS_DOCKER),1)
	@echo "Using existing version.c: $(shell sh -c 'cat version.c')"
else
	@echo selva_db_version $(SELVA_DB_VERSION)
	echo "const char * const selva_db_version = \"$(SELVA_DB_VERSION)\";" > version.c
endif

FORCE:
