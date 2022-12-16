# Copyright (c) 2022 SAULX
#
# SPDX-License-Identifier: MIT

CFLAGS := -std=gnu2x -g -O1 -include ../../../include/cdefs.h -I../../../include

strrnchr: strrnchr.c ../cstrings.c
	clang $(CFLAGS) -fsanitize=fuzzer,signed-integer-overflow $^ -o $@

get_array_field_index: get_array_field_index.c ../cstrings.c
	clang $(CFLAGS) -fsanitize=fuzzer,signed-integer-overflow $^ -o $@

selva_proto_parse_vtype: get_array_field_index.c ../cstrings.c
	clang $(CFLAGS) -fsanitize=fuzzer,signed-integer-overflow $^ -o $@
