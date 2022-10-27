# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

include ../common.mk
CFLAGS += -fPIC -g -fvisibility=hidden -I../../include -include ../include/cdefs.h
ifeq ($(uname_S),Linux)
LDFLAGS += -g -shared -fPIC
endif
ifeq ($(uname_S),Darwin)
LDFLAGS += -g -bundle
endif
export

MODULES := \
		   demo_await \
		   demo_sock \
		   demo_timeout \
		   server \
		   signal

all: $(MODULES)

$(MODULES):
	$(MAKE) -C "$@"

.PHONY: $(MODULES)
