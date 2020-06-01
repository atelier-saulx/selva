# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

RM_INCLUDE_DIR := ./ ./rmutil ./util
RMUTIL_LIBDIR := rmutil
UTIL_LIBDIR := util

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?=  -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Bsymbolic
	SHOBJ_LDLIBS := -lrmutil -lutil -lc  -luuid -lhiredis -lcrypto -lssl
else
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
endif
CFLAGS := $(patsubst %,-I%, $(RM_INCLUDE_DIR)) -Wall -g -fPIC -lc -lm -std=gnu99
CC := gcc


all: rmutil util module.so

rmutil: FORCE
	$(MAKE) -C $(RMUTIL_LIBDIR)

util: FORCE
	$(MAKE) -C ./util

module.so: module.o id/id.o modify/modify.o modify/async_task.o
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(LIBS) -L$(UTIL_LIBDIR) -L$(RMUTIL_LIBDIR) -L/usr/local/Cellar/openssl@1.1/1.1.1g/lib/ $(SHOBJ_LDLIBS)

clean: FORCE
	find . -type f -name "*.a" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . -type f -name "*.so" -exec rm -f {} \;
	find . -type f -name "*.xo" -exec rm -f {} \;

FORCE:
