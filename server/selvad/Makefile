# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

SHELL := /bin/bash
_MOD_PATH := "$(PWD)/modules"

include common.mk
export uname_S

# Ordered list of libraries
LIBS := \
		lib/jemalloc \
		lib/deflate \
		lib/util

all: selvad modules $(LIBS)

selvad: export MOD_PATH = $(_MOD_PATH)
selvad:
	$(MAKE) -C src

# Build all libraries (ordered)
# TODO Doesn't work properly!?
lib: | $(LIBS)

$(LIBS):
	$(MAKE) -C $@

modules: export MOD_PATH = $(_MOD_PATH)
modules:
	$(MAKE) -C modules

clean:
	$(RM) selvad
	find . -type f -name "*.d" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . -type f -name "*.so" -exec rm -f {} \;
	find . -type f -name "*.dylib" -exec rm -f {} \;
	find ./lib -type d -maxdepth 1 -exec $(MAKE) -C {} clean \;

.PHONY: clean selvad modules lib $(LIBS)
