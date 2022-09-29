# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_P := $(shell sh -c 'uname -p 2>/dev/null || echo not')

CFLAGS := -std=gnu99 -Wall -Wextra -Wimplicit-fallthrough -Wno-unused-value -fvisibility=hidden

# Compile flags for linux / osx
ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
	CFLAGS += -g -ggdb3 -fno-math-errno -ftree-vectorize
	#-opt-info-vec-optimized
	#-ftree-vectorizer-verbose=5 -fopt-info-vec-missed

	ifeq ($(uname_P),x86_64)
		CCFLAGS += -march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2 -mbmi -mbmi2 -mlzcnt -mmovbe -mprfchw
	endif
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
	CFLAGS += -g -Wno-zero-length-array -Wno-c11-extensions -Wno-unknown-attributes

	ifeq ($(uname_P),i386)
		CFLAGS += -march=x86-64 -mtune=core-avx2 -mfpmath=sse -mavx -mavx2
	endif
	ifeq ($(uname_P),arm64)
		# We use Rosetta2
		CFLAGS += -march=x86-64
	endif
endif
