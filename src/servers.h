#ifndef SERVERS_H
#define SERVERS_H

#include <pthread.h>

extern pthread_mutex_t serversMutex;

typedef struct {
    char* hostname;
    char* destination;
    short port;
} Entry;

void load_dictionary(const char* filename);
Entry* find_entry(const char* key);

#endif // SERVERS_H