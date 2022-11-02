CFLAGS := -Wall -Wextra -g3 -std=gnu2x -I../../include

dbgcli: dbgcli.o
	$(CC) -L../../lib -lutil -ljemalloc_selva -Wl,-rpath,'$$ORIGIN'/../../lib -o $@ $^

clean:
	$(RM) ./dbgcli dbgcli.o
