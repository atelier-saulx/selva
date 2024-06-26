include common.mk

SELVA_VERSION := $(shell sh -c 'git rev-parse --verify HEAD')
INCLUDE_DIR := ./include ./util
RMUTIL_LIBDIR := rmutil
UTIL_LIBDIR := util
OBJS := \
	version.o \
	module.o \
	module/alias.o \
	module/arg_parser.o \
	module/async_task.o \
	module/comparator.o \
	module/config.o \
	module/edge.o \
	module/edge_constraint.o \
	module/errors.o \
	module/find.o \
	module/find_index.o \
	module/aggregate.o \
	module/traversal.o \
	module/hierarchy.o \
	module/hierarchy_reply.o \
	module/inherit.o \
	module/modify.o \
	module/resolve.o \
	module/rpn.o \
	module/rpn_eval.o \
	module/selva_lang.o \
	module/selva_node.o \
	module/selva_object.o \
	module/selva_set.o \
	module/selva_type.o \
	module/subscriptions.o

IDIR := $(patsubst %,-I%, $(INCLUDE_DIR))

CFLAGS += $(IDIR) -include tunables.h \
	-O2 \
	-fPIC \
	-fno-strict-aliasing \
	-D REDISMODULE_EXPERIMENTAL_API=1
# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Bsymbolic
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
else ifeq ($(shell test `sw_vers -productVersion|cut -c1-2` -ge 11; echo $$?),0)
#	CFLAGS += -Wpadded
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup -L /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
else
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
	SHOBJ_LDLIBS := -lrmutil -lutil -lc -lhiredis -lcrypto -lssl
endif
CC ?= gcc


all: module.so

version.c: FORCE
ifeq ($(SELVA_IS_DOCKER),1)
	@echo "Using existing version.c: $(cat version.c)"
else
	@echo selva_version $(SELVA_VERSION)
	echo "const char * const selva_version = \"$(SELVA_VERSION)\";" > version.c
endif

$(RMUTIL_LIBDIR)/librmutil.a: FORCE
	$(MAKE) -C $(RMUTIL_LIBDIR)

$(UTIL_LIBDIR)/libutil.a: FORCE
	$(MAKE) -C $(UTIL_LIBDIR)

module.so: $(OBJS) $(RMUTIL_LIBDIR)/librmutil.a $(UTIL_LIBDIR)/libutil.a
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(LIBS) -L$(UTIL_LIBDIR) -L$(RMUTIL_LIBDIR) -L/usr/local/Cellar/openssl@1.1/1.1.1k/lib/ $(SHOBJ_LDLIBS)

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

test-gcov: test
	./coverage.sh

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
