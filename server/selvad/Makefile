# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

SHELL := /bin/bash

include common.mk
export uname_S

INSTALL_DIR := binaries/$(uname_S)_$(uname_M)

# Ordered list of libraries
LIBS := \
		lib/deflate \
		lib/jemalloc \
		lib/sha3iuf \
		lib/util

all: selvad modules $(LIBS)

selvad: lib
	$(MAKE) -C src

# Build all libraries (ordered)
# TODO Doesn't work properly!?
lib: | $(LIBS)

$(LIBS):
	$(MAKE) -C $@

modules: lib
	$(MAKE) -C modules

install: all
	mkdir -p "$(INSTALL_DIR)/lib"
	mkdir -p "$(INSTALL_DIR)/modules"
	mkdir -p "$(INSTALL_DIR)/locale"
	install -s $(wildcard lib/lib*) "$(INSTALL_DIR)/lib"
	install -s $(wildcard modules/*$(MOD_SUFFIX)) "$(INSTALL_DIR)/modules"
	install -s selvad "$(INSTALL_DIR)/"
	$(MAKE) -C locale LOCPATH=../$(INSTALL_DIR)/locale

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
	$(MAKE) -C test clean

clean:
	$(RM) selvad
	find . -type f -name "*.d" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . \( -type l -o -type l \) -name "*.so" -exec rm -f {} \;
	find . \( -type l -o -type l \) -name "*.dylib" -exec rm -f {} \;
	$(MAKE) -C test clean
	find ./lib -type d -maxdepth 1 -exec $(MAKE) -C {} clean \;

.PHONY: all clean check test mostlyclean selvad modules lib $(LIBS)
.NOTPARALLEL:
