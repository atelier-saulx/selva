# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

TARGET := ../demo_timeout$(MOD_SUFFIX)

$(TARGET): demo.o
	$(CC) -o $@ $(LDFLAGS) $^

-include demo.d
