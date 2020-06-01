RM_INCLUDE_DIR := ../

CFLAGS ?= -g -fPIC -O3 -std=gnu99 -Wall -Wno-unused-function
CFLAGS += -I$(RM_INCLUDE_DIR)
CC=gcc

OBJS=queue_r.o

all: libutil.a

libutil.a: $(OBJS)
	ar rcs $@ $^
