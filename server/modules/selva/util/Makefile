RM_INCLUDE_DIR := ../include

CFLAGS ?= -g -fPIC -O3 -std=gnu99 -Wall -Wno-unused-function
CFLAGS += -I$(RM_INCLUDE_DIR)
CC=gcc

OBJS=queue_r.o svector.o

all: libutil.a

libutil.a: $(OBJS)
	ar rcs $@ $^
