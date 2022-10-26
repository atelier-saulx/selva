# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

TARGET := ../libdemo_sock$(MOD_SUFFIX)

$(TARGET): demo.o
	$(CC) -o $@ $(LDFLAGS) $^

-include demo.d
