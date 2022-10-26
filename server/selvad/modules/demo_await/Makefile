# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

TARGET := ../libdemo_async$(MOD_SUFFIX)

$(TARGET): demo.o
	$(CC) -o $@ -shared -fPIC -g $^

-include demo.d
