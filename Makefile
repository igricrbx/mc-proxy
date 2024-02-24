CC=gcc
CFLAGS=-pthread -lresolv -O3

proxy: src/main.c
	$(CC) -o bin/proxy src/* $(CFLAGS)

clean:
	rm -f bin/proxy