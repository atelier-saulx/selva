# Copyright (c) 2022-2023 SAULX
# SPDX-License-Identifier: MIT

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
OBJ := \
	   perf.o
CFLAGS := -Wall -Wextra -g3 -std=gnu2x -I../../include
LDFLAGS := -L../../lib -lm -lutil -ljemalloc_selva -ldeflate
ifeq ($(uname_S),Linux)
LDFLAGS += -Wl,-rpath,'$$ORIGIN'/../../lib
endif
ifeq ($(uname_S),Darwin)
LDFLAGS += -Wl,-rpath,@executable_path/../../lib
endif

perf: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	$(RM) ./perf $(OBJ)
