include common.mk

SELVA_VERSION := $(shell sh -c 'git rev-parse --verify HEAD')
INCLUDE_DIR := include lib/util lib/deflate
LIBS := rmutil util deflate
OBJS := \
	version.o \
	module.o \
	module/aggregate.o \
	module/alias.o \
	module/arg_parser.o \
	module/async_task.o \
	module/comparator.o \
	module/config.o \
	module/edge/edge.o \
	module/edge/edge_constraint.o \
	module/errors.o \
	module/find.o \
	module/find_index/find_index.o \
	module/hierarchy/field_set.o \
	module/hierarchy/hierarchy.o \
	module/hierarchy/hierarchy_detached.o \
	module/hierarchy/hierarchy_reply.o \
	module/hierarchy/traversal.o \
	module/ida.o \
	module/inherit.o \
	module/modify.o \
	module/modinfo.o \
	module/resolve.o \
	module/rms/rms_compressor.o \
	module/rms/shared.o \
	module/rpn/rpn.o \
	module/rpn/rpn_eval.o \
	module/selva_lang.o \
	module/selva_object/selva_object.o \
	module/selva_object/selva_object_commands.o \
	module/selva_object/selva_object_foreach.o \
	module/selva_set/field_has.o \
	module/selva_set/fielda_in_fieldb.o \
	module/selva_set/fielda_in_setb.o \
	module/selva_set/selva_set.o \
	module/selva_set/seta_in_fieldb.o \
	module/selva_set/seta_in_setb.o \
	module/selva_trace.o \
	module/selva_type.o \
	module/subscriptions.o \
	module/timestamp.o

IDIR := $(patsubst %,-I%,$(INCLUDE_DIR))
CFLAGS += $(IDIR) -include tunables.h \
	-O2 \
	-fPIC \
	-fno-strict-aliasing
LIBDIR := $(patsubst %,lib/%,$(LIBS))
SHOBJ_LDLIBS := -lc -lhiredis -lcrypto -lssl
# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	CFLAGS += -include include/linux_cdefs.h \
			  -fisolate-erroneous-paths-attribute
	SHOBJ_CFLAGS ?= -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Bsymbolic
endif
ifeq ($(uname_S),Darwin) # macOS
	CFLAGS += -include include/macos_cdefs.h
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
	LIBDIR += /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib /usr/local/Cellar/openssl@1.1/1.1.1k/lib
endif
ifeq ($(SELVA_TRACE),1)
	CFLAGS += -I/opt/intel/oneapi/vtune/2021.7.1/sdk/include -DSELVA_TRACE=1
	SHOBJ_LDFLAGS += /opt/intel/oneapi/vtune/2021.7.1/sdk/lib64/libittnotify.a
endif
CC ?= gcc
LIBS_A := $(foreach S,$(LIBS),lib/$(S)/lib$(S).a)

all: module.so

version.c: FORCE
ifeq ($(SELVA_IS_DOCKER),1)
	@echo "Using existing version.c: $(cat version.c)"
else
	@echo selva_version $(SELVA_VERSION)
	echo "const char * const selva_version = \"$(SELVA_VERSION)\";" > version.c
endif

$(LIBS_A): FORCE
	@if [[ $(@) == *"deflate"* ]]; then \
		$(MAKE) DISABLE_SHARED=1 CFLAGS=-fPIC DISABLE_GZIP=1 DISABLE_ZLIB=1 -C $(dir $@) libdeflate.a; \
	else \
		$(MAKE) -C $(dir $@); \
	fi

module.so: $(OBJS) $(LIBS_A)
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(addprefix  -L,$(LIBDIR)) $(SHOBJ_LDLIBS)

check:
ifeq ($(uname_S),Linux)
	cppcheck --quiet --inline-suppr \
		--language=c --std=c99 --platform=unix64 --enable=all \
		-addon=threadsafety.py --addon=cert.py \
		--suppressions-list=cppcheck_suppressions.txt \
		--library=redis_cppcheck.cfg --library=selva_cppcheck.cfg \
		$(IDIR) module.c ./module ./lib/util
else # Assume macOs
	cppcheck --quiet --inline-suppr \
		--language=c --std=c99 --platform=unix64 --enable=all \
		--suppressions-list=cppcheck_suppressions.txt \
		--library=redis_cppcheck.cfg --library=selva_cppcheck.cfg \
		-i test $(IDIR) -I./module -I./lib/util module.c ./module ./lib/util
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
	$(MAKE) -C lib/deflate clean

FORCE:

.PHONY: check test
