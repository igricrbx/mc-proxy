#ifndef SERVERS_H
#define SERVERS_H

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    char source[256];
    char destination[256];
    short port;
} Entry;

ssize_t load_dictionary(const char* filename);
Entry* find_entry(const char* key);

#endif // SERVERS_H