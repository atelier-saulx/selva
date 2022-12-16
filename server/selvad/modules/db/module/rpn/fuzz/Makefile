# Copyright (c) 2022 SAULX
#
# SPDX-License-Identifier: MIT

CFLAGS := -std=gnu2x -g -O1 \
		  -include ../../../../../include/cdefs.h\
		  -include ../../../../../tunables.h \
		  -I../../../../../include -I../../../include \
		  -mfpmath=sse \
		  -fno-math-errno \
		  -Wno-unknown-attributes
SRC := ../rpn.c \
	   ../../selva_set/selva_set.c \
	   ../../selva_set/seta_in_setb.c \
	   ../../selva_object/selva_object.c \
	   ../../../../../lib/util/selva_string.c \
	   ../../../../../lib/util/svector.c \
	   ../../../../../lib/util/cstrings.c \
	   ../../../../../lib/util/mempool.c \
	   ../../../../../lib/util/finalizer.c

rpn: rpn.c $(SRC)
	clang $(CFLAGS) -fsanitize=fuzzer,signed-integer-overflow,address  $^ -o $@
