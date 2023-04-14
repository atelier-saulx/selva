# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT
include ../../common.mk

CFLAGS += -fPIC -I../../include/sha3iuf
ifeq ($(uname_S),Linux)
	LDFLAGS += -shared -fPIC
endif
ifeq ($(uname_S),Darwin)
	LDFLAGS += -shared -Wl,-install_name,@rpath/libsha3iuf$(LIB_SUFFIX)
endif
LDLIBS :=

ifndef LIB_SUFFIX
$(error LIB_SUFFIX is not set)
endif
TARGET := ../libsha3iuf$(LIB_SUFFIX)

all: $(TARGET)
#all: $(TARGET) sha3sum sha3test

.SUFFIXES: .c .o
.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): sha3.o 
	$(CC) -o $@ $(LDFLAGS) $^

sha3sum: sha3.o sha3sum.o
	$(CC) $(LDFLAGS) -o $@ sha3.o sha3sum.o $(LDLIBS)

sha3fuzz: sha3.c fuzz/sha3fuzz.c
	clang -g --std=c99 -fsanitize=fuzzer,address -O0 -I../../include/sha3iuf -o $@ $^

sha3test: sha3.o sha3test.o
	$(CC) $(LDFLAGS) -o $@ sha3.o sha3test.o $(LDLIBS)

check: sha3test
	./sha3test

clean:
	rm -f *.o libsha3.a sha3sum sha3test sha3fuzz

.PHONY: all check clean install uninstall
