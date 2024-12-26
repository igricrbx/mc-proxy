#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include "../dns.h"
#include "../packet-tools.h"
#include "../servers.h"

void test_dns_query() {
    char output_address[16];
    ssize_t result;

    result = dns_query_mc("one.one.one.one", output_address);

    assert(result == 0);
    assert(strcmp(output_address, "1.0.0.1") == 0);

    result = dns_query_mc("localhost", output_address);

    assert(result == 0);
    assert(strcmp(output_address, "127.0.0.1") == 0);

    result = dns_query_mc("wynncraft.com", output_address);

    assert(result == 0);
    assert(strcmp(output_address, "72.251.7.223") == 0);

    result = dns_query_mc("play.wynncraft.com", output_address);

    assert(result == 0);
    assert(strcmp(output_address, "72.251.7.223") == 0);

    result = dns_query_mc("1.12.123.234", output_address);

    assert(result == 0);
    assert(strcmp(output_address, "1.12.123.234") == 0);
}

void test_resolve_hostname() {
    char output_address[16];

    size_t cache_size = 2;
    ssize_t result = dns_cache_init(cache_size);

    assert(result == 0);

    ssize_t error = resolve_hostname("wynncraft.com", output_address);
    error += resolve_hostname("wynncraft.com", output_address);
    error += resolve_hostname("wynncraft.com", output_address);
    error += resolve_hostname("wynncraft.com", output_address);
    error += resolve_hostname("play.wynncraft.com", output_address);
    error += resolve_hostname("play.wynncraft.com", output_address);
    error += resolve_hostname("one.one.one.one", output_address);
    error += resolve_hostname("one.one.one.one", output_address);

    assert(error == 0);
}

void test_server_dictionary() {
    ssize_t result = load_dictionary("servers.conf");

    assert(result == 0);

    Entry* entry = find_entry("minecraft.local.igric");

    assert(entry != NULL);
    assert(strcmp(entry->source, "minecraft.local.igric") == 0);
    assert(strcmp(entry->destination, "wynncraft.com") == 0);
    assert(entry->port == 25565);

    entry = find_entry("pi.igric");

    assert(entry != NULL);
    assert(strcmp(entry->source, "pi.igric") == 0);
    assert(strcmp(entry->destination, "192.168.1.5") == 0);
    assert(entry->port == 25577);
}

int main(void) {
    test_dns_query();
    test_resolve_hostname();
    test_server_dictionary();

    printf("All tests passed\n");
    return 0;
}