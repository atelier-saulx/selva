# Copyright (c) 2022-2023 SAULX
# SPDX-License-Identifier: MIT

include ../common.mk
CFLAGS += -fPIC -g -fvisibility=hidden -I../../include -include ../../include/cdefs.h
LDFLAGS += -L../../lib
ifeq ($(uname_S),Linux)
# Note that double double $ is needed because `export` will parse the string
# again and would otherwise try to locate the variable named `$O`.
# FIXME This should use $ORIGIN
LDFLAGS += -g -shared -Wl,-rpath,'$(PWD)/lib'
#-Wl,-rpath,'$$$$ORIGIN'/../lib
endif
ifeq ($(uname_S),Darwin)
LDFLAGS += -g -bundle
endif
export

MODULES := \
		   db \
		   demo_await \
		   demo_sock \
		   demo_timeout \
		   io \
		   replication \
		   server \
		   signal

all: $(MODULES)

$(MODULES):
	$(MAKE) -C "$@"

.PHONY: $(MODULES)
