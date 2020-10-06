RM_INCLUDE_DIR := ../include

CFLAGS ?= -g -fPIC -O3 -std=gnu99 -Wall -Wno-unused-function -Wno-unused-value
CFLAGS += -I$(RM_INCLUDE_DIR)
CC=gcc

OBJS := cstrings.o queue_r.o svector.o trx.o

all: libutil.a

libutil.a: $(OBJS)
	ar rcs $@ $^
