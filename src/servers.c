#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "servers.h"

pthread_mutex_t serversMutex;

Entry* dictionary = NULL;
size_t dictionary_size = 0;


char* trim_leading_whitespace(char* str) {
    while(*str == ' ' || *str == '\t') {
        ++str;
    }
    return str;
}

void add_entry(char* key, char* value, short port) {
    dictionary = realloc(dictionary, sizeof(Entry) * (dictionary_size + 1));
    if (dictionary == NULL) {
        printf("Failed to allocate memory for dictionary\n");
        return;
    }

    dictionary[dictionary_size].hostname = key;
    dictionary[dictionary_size].destination = value;
    dictionary[dictionary_size].port = port;
    ++dictionary_size;
}


void load_dictionary(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        if (line[0] == '#') {
            continue;
        }

        char* key = strtok(line, " \t");
        char* value = strtok(NULL, "\n");
        if (key && value) {
            char* destination = strtok(value, ":");
            destination = trim_leading_whitespace(destination);
            char* port_str = strtok(NULL, "");
            short port = port_str ? atoi(port_str) : 25565;
            add_entry(strdup(key), strdup(destination), port);
        }
    }

    free(line);
    fclose(file);
}

Entry* find_entry(const char* key) {
    pthread_mutex_lock(&serversMutex);

    for (size_t i = 0; i < dictionary_size; i++) {
        if (strcmp(dictionary[i].hostname, key) == 0) {
            pthread_mutex_unlock(&serversMutex);
            return &dictionary[i];
        }
    }

    // Wildcard match
    char* domain = strchr(key, '.');
    if (domain) {
        char wildcard_hostname[256];
        snprintf(wildcard_hostname, sizeof(wildcard_hostname), "*%s", domain);

        for (size_t i = 0; i < dictionary_size; i++) {
            if (strcmp(dictionary[i].hostname, wildcard_hostname) == 0) {
                pthread_mutex_unlock(&serversMutex);
                return &dictionary[i];
            }
        }
    }

    pthread_mutex_unlock(&serversMutex);

    return NULL;
}