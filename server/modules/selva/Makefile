# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

INCLUDE_DIR := ./include ./util
RMUTIL_LIBDIR := rmutil
UTIL_LIBDIR := util
OBJS := \
	module.o \
	module/alias.o \
	module/arg_parser.o \
	module/async_task.o \
	module/comparator.o \
	module/errors.o \
	module/find.o \
	module/hierarchy.o \
	module/hierarchy_reply.o \
	module/inherit.o \
	module/modify.o \
	module/resolve.o \
	module/rpn.o \
	module/rpn_eval.o \
	module/selva_node.o \
	module/selva_object.o \
	module/selva_set.o \
	module/subscriptions.o

IDIR := $(patsubst %,-I%, $(INCLUDE_DIR))

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Bsymbolic
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
else ifeq ($(shell test `sw_vers -productVersion|cut -c1-2` -ge 11; echo $$?),0)
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup -L /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
else
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
endif
CFLAGS := $(IDIR) -include tunables.h \
	-std=gnu99 -fPIC \
	-Wall -Wextra -Wno-unused-value -Wno-zero-length-array -Wno-c11-extensions \
	-ggdb -O2 \
	-fno-strict-aliasing \
	-march=x86-64 -mtune=intel -mfpmath=sse -mavx -mavx2
CC := gcc


all: module.so

$(RMUTIL_LIBDIR)/librmutil.a: FORCE
	$(MAKE) -C $(RMUTIL_LIBDIR)

$(UTIL_LIBDIR)/libutil.a: FORCE
	$(MAKE) -C $(UTIL_LIBDIR)

module.so: $(OBJS) $(RMUTIL_LIBDIR)/librmutil.a $(UTIL_LIBDIR)/libutil.a
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(LIBS) -L$(UTIL_LIBDIR) -L$(RMUTIL_LIBDIR) -L/usr/local/Cellar/openssl@1.1/1.1.1i/lib/ $(SHOBJ_LDLIBS)

check:
ifeq ($(uname_S),Linux)
	cppcheck --quiet --inline-suppr \
		--language=c --std=c99 --platform=unix64 --enable=all \
		-addon=threadsafety.py --addon=cert.py \
		--suppressions-list=cppcheck_suppressions.txt \
		--library=redis_cppcheck.cfg --library=selva_cppcheck.cfg \
		-i $(RMUTIL_LIBDIR) $(IDIR) .
else # Assume macOs
	cppcheck --quiet --inline-suppr \
		--language=c --std=c99 --platform=unix64 --enable=all \
		--suppressions-list=cppcheck_suppressions.txt \
		--library=redis_cppcheck.cfg --library=selva_cppcheck.cfg \
		-i $(RMUTIL_LIBDIR) -i test $(IDIR) -I./module .
endif

test:
	$(MAKE) -C test

test-valgrind: export WITH_VALGRIND = valgrind
test-valgrind:
	$(MAKE) -C test

clean: FORCE
	find . -type f -name "*.a" -exec rm -f {} \;
	find . -type f -name "*.o" -exec rm -f {} \;
	find . -type f -name "*.so" -exec rm -f {} \;
	find . -type f -name "*.xo" -exec rm -f {} \;
	find . -type f -name "*.gcda" -exec rm -f {} \;
	find . -type f -name "*.gcno" -exec rm -f {} \;
	find . -type f -name "*.gcov" -exec rm -f {} \;
	$(MAKE) -C test clean

FORCE:

.PHONY: check test
