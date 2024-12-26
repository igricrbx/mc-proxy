#ifndef DNS_H
#define DNS_H

#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <arpa/nameser.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    char hostname[256];
    char ip_address[16];
    time_t last_used;
} CacheEntry;

ssize_t dns_cache_init(size_t cache_size);
void dns_cache_destroy();
ssize_t resolve_hostname(const char* const hostname, char* const address);
ssize_t dns_query_mc(const char* const fqdn, char* const address);

#endif // DNS_H