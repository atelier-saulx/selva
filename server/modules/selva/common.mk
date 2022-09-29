# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_P := $(shell sh -c 'uname -p 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')

CFLAGS := -std=gnu99 -Wall -Wextra -Wimplicit-fallthrough -Wno-unused-value -fvisibility=hidden

# Compile flags for linux / osx
ifeq ($(uname_S),Linux) # Assume Intel x86-64 Linux
	CFLAGS += -g -ggdb3 -fno-math-errno -ftree-vectorize
	#-opt-info-vec-optimized
	#-ftree-vectorizer-verbose=5 -fopt-info-vec-missed

	ifeq ($(uname_M),x86_64)
		CCFLAGS += -march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2 -mbmi -mbmi2 -mlzcnt -mmovbe -mprfchw
	endif
endif
ifeq ($(uname_S),Darwin) # Assume x86-64 macOS
	CFLAGS += -g -Wno-zero-length-array -Wno-c11-extensions -Wno-unknown-attributes
	ROSETTA2 := $(shell sh -c 'sysctl -n sysctl.proc_translated 2>/dev/null || echo 0')

	ifeq ($(uname_M),x86_64)
		CFLAGS += -march=x86-64
		ifeq ($(ROSETTA2),0)
			CFLAGS += -mtune=core-avx2 -mfpmath=sse -mavx -mavx2
		endif
	endif
	ifeq ($(uname_M),arm64)
		# We use Rosetta 2
		CFLAGS += -march=x86-64
	endif
endif
