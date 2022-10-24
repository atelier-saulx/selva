uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
CFLAGS := -g3 -MMD -Wall -Wextra
