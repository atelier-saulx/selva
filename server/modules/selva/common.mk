# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

CFLAGS := -std=gnu99 -Wall -Wextra -Wno-unused-value -Wno-zero-length-array -Wno-c11-extensions

# Compile flags for linux / osx
ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
	CFLAGS += -g -ggdb3 -march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2 -mbmi -mbmi2 -mlzcnt -mmovbe -mprfchw -fno-math-errno -ftree-vectorize
	#-opt-info-vec-optimized
	#-ftree-vectorizer-verbose=5 -fopt-info-vec-missed
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
	CFLAGS += -g -march=x86-64 -mtune=core-avx2 -mfpmath=sse -mavx -mavx2
endif
