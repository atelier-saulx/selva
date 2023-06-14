# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

# Anything defined here will generally shared by all build goals except
# libraries, unless the library Makefile explicitly imports this file.

# OS name (Linux, Darwin)
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')

# Set _DATE__ and __TIME__ macros to a deterministic value
export SOURCE_DATE_EPOCH := $(shell sh -c 'git log -1 --pretty=%ct || date +%s')
export ZERO_AR_DATE := 1

# CFLAGS shared with all compilation units.
# TODO gnu23 when available
CFLAGS := -std=gnu2x -O2 -fno-strict-aliasing -MMD -Wall -Wextra

ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
	CFLAGS += -g -ggdb3 -fno-math-errno -ftree-vectorize
	#-opt-info-vec-optimized
	#-ftree-vectorizer-verbose=5 -fopt-info-vec-missed

	ifeq ($(uname_M),x86_64)
		CFLAGS += -march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2 -mbmi -mbmi2 -mlzcnt -mmovbe -mprfchw
	endif

	LIB_SUFFIX := .so
	MOD_SUFFIX := .so
endif
ifeq ($(uname_S),Darwin) # Assume macOS
	ROSETTA2 := $(shell sh -c 'sysctl -n sysctl.proc_translated 2>/dev/null || echo 0')

	CFLAGS += -g -Wno-zero-length-array -Wno-c11-extensions -Wno-unknown-attributes

	ifeq ($(uname_M),x86_64)
		CFLAGS += -march=x86-64
		ifeq ($(ROSETTA2),0)
			CFLAGS += -mtune=core-avx2 -mfpmath=sse -mavx -mavx2
		endif
	endif
	ifeq ($(uname_M),arm64)
		CFLAGS += -mcpu=apple-m1
	endif

	LIB_SUFFIX := .dylib
	MOD_SUFFIX := .so
endif
