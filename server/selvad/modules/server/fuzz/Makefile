# Copyright (c) 2022 SAULX
#
# SPDX-License-Identifier: MIT

CFLAGS := -std=gnu2x -g -O1 \
		  -include ../../../include/cdefs.h\
		  -include ../../../tunables.h \
		  -DFUZZER \
		  -I../../../include -I../../../include \
		  -mfpmath=sse \
		  -fno-math-errno \
		  -Wno-unknown-attributes
SRC := ../server_frame.c \
	   ../../../lib/util/selva_proto.c

server_frame: server_frame.c $(SRC)
	clang $(CFLAGS) -fsanitize=fuzzer,signed-integer-overflow,address $^ -o $@

server_msg: server_msg.c $(SRC) ../server_msg.c
	clang $(CFLAGS) -fsanitize=fuzzer,signed-integer-overflow,address $^ -o $@
