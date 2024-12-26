#include "servers.h"

pthread_mutex_t servers_mutex;

typedef struct {
    Entry* items;
    size_t count;
    size_t capacity;
} Dictionary;

Dictionary dictionary = {0};


char* trim_leading_whitespace(char* str) {
    while(*str == ' ' || *str == '\t') {
        ++str;
    }
    return str;
}

ssize_t add_entry(const char* const source, const char* const destination, const short port) {
    if (dictionary.count >= dictionary.capacity) {
        if (dictionary.capacity == 0) {
            dictionary.capacity = 8;
        } else {
            dictionary.capacity *= 2;
        }
        dictionary.items = realloc(dictionary.items, sizeof(Entry) * dictionary.capacity);
        if (dictionary.items == NULL) {
            perror("Error allocating memory");
            return -1;
        }
    }
    
    strncpy(dictionary.items[dictionary.count].source, source, sizeof(dictionary.items[dictionary.count].source));
    strncpy(dictionary.items[dictionary.count].destination, destination, sizeof(dictionary.items[dictionary.count].destination));
    dictionary.items[dictionary.count].port = port;

    ++dictionary.count;
    return 0;
}


ssize_t load_dictionary(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open the file\n");
        return -1;
    }

    if (pthread_mutex_init(&servers_mutex, NULL) != 0) {
        perror("Error creating servers mutex");
        return -1;
    }

    char* line = NULL;
    size_t len;
    while (getline(&line, &len, file) != -1) {
        if (line[0] == '#') {
            continue;
        }

        char* source = strtok(line, " \t");
        char* value = strtok(NULL, "\n");
        if (source && value) {
            char* destination = strtok(value, ":");
            destination = trim_leading_whitespace(destination);
            char* port_str = strtok(NULL, "");
            short port = port_str ? atoi(port_str) : 25565;
            if (add_entry(source, destination, port) < 0) {
                perror("Error adding entry");
                return -1;
            }
        }
    }

    if (fclose(file) != 0) {
        perror("Error closing the file");
        return -1;
    }

    return 0;
}

Entry* find_entry(const char* key) {
    pthread_mutex_lock(&servers_mutex);

    for (size_t i = 0; i < dictionary.count; i++) {
        if (strcmp(dictionary.items[i].source, key) == 0) {
            pthread_mutex_unlock(&servers_mutex);
            return &dictionary.items[i];
        }
    }

    // Wildcard match
    char* domain = strchr(key, '.');
    if (domain) {
        char wildcard_source[256];
        snprintf(wildcard_source, sizeof(wildcard_source), "*%s", domain);

        for (size_t i = 0; i < dictionary.count; i++) {
            if (strcmp(dictionary.items[i].source, wildcard_source) == 0) {
                pthread_mutex_unlock(&servers_mutex);
                return &dictionary.items[i];
            }
        }
    }

    pthread_mutex_unlock(&servers_mutex);

    return NULL;
}