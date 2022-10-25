# Copyright (c) 2022 SAULX
# SPDX-License-Identifier: MIT

OBJ := \
	   main.o \
	   trace.o

DEP := $(OBJ:%.o=%.d)

../libsignal.so: $(OBJ)
	$(CC) -o $@ -shared -fPIC -g $^

-include $(DEP)
