# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

# Anything defined here will generally shared by all build goals except
# libraries, unless the library Makefile explicitly imports this file.

# OS name (Linux, Darwin)
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Set _DATE__ and __TIME__ macros to a deterministic value
export SOURCE_DATE_EPOCH := $(git log -1 --pretty=%ct)
export ZERO_AR_DATE := 1

# CFLAGS shared with all compilation units.
CFLAGS := -MMD -Wall -Wextra
# See substitute-path in .gdbinit
CFLAGS += -ffile-prefix-map=$(PWD)=/src

ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
	CFLAGS += -g -ggdb3 -march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2 -mbmi -mbmi2 -mlzcnt -mmovbe -mprfchw -fno-math-errno -ftree-vectorize
	#-opt-info-vec-optimized
	#-ftree-vectorizer-verbose=5 -fopt-info-vec-missed
	LIB_SUFFIX := .so
	MOD_SUFFIX := .so
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
	CFLAGS += -g -march=x86-64 -mtune=core-avx2 -mfpmath=sse -mavx -mavx2 -Wno-zero-length-array -Wno-c11-extensions -Wno-unknown-attributes
	LIB_SUFFIX := .dylib
	MOD_SUFFIX := .so
endif
