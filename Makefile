CC=gcc
CFLAGS=-pthread -lresolv -O3

proxy: src/main.c
	$(CC) -std=c11 -o bin/proxy src/*.c $(CFLAGS)

tests: src/tests/tests.c
	$(CC) -std=c11 -o bin/tests src/tests/tests.c src/dns.c src/packet-tools.c src/servers.c $(CFLAGS)

clean:
	rm -f bin/proxy