# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

CFLAGS := -std=gnu99 -Wall -Wextra -Wno-unused-value -Wno-zero-length-array -Wno-c11-extensions

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	CFLAGS += -ggdb3 -march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2
else # Assume x86-64 mac
	CFLAGS += -g -march=x86-64 -mtune=core-avx2 -mfpmath=sse -mavx -mavx2
endif


