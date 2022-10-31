# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

SHELL := /bin/bash

include common.mk
export uname_S

# Ordered list of libraries
LIBS := \
		lib/jemalloc \
		lib/deflate \
		lib/util

all: selvad modules $(LIBS)

selvad:
	$(MAKE) -C src

# Build all libraries (ordered)
# TODO Doesn't work properly!?
lib: | $(LIBS)

$(LIBS):
	$(MAKE) -C $@

modules:
	$(MAKE) -C modules

test:
	$(MAKE) -C test

test-valgrind: export WITH_VALGRIND = valgrind
test-valgrind:
	$(MAKE) -C test

test-gcov: test
	./coverage.sh

check:
	cppcheck src/ modules/

mostlyclean:
	$(RM) selvad
	find ./src -type f -name "*.d" -exec rm -f {} \;
	find ./src -type f -name "*.o" -exec rm -f {} \;
	find ./modules -type f -name "*.d" -exec rm -f {} \;
	find ./modules -type f -name "*.o" -exec rm -f {} \;
	find ./modules -type f -name "*.so" -exec rm -f {} \;

clean:
	$(RM) selvad
	find . -type f -name "*.d" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . -type f -name "*.so" -exec rm -f {} \;
	find . -type f -name "*.dylib" -exec rm -f {} \;
	$(MAKE) -C test clean
	find ./lib -type d -maxdepth 1 -exec $(MAKE) -C {} clean \;

.PHONY: all clean check test mostlyclean selvad modules lib $(LIBS)
