#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "packet-tools.h"
#include "dns.h"

#define STATE_LOGIN 0x02

struct CacheEntry dns_cache[CACHE_SIZE];

char* resolve_hostname(const char* hostname) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, hostname, &(sa.sin_addr));
    // if the hostname is already an IP address, no need to query the DNS
    if (result != 0)
    {
        return strdup(hostname);
    }
    // localhost should resolve to 127.0.0.1
    if (!strcmp(hostname, "localhost")) {
        return strdup("127.0.0.1");
    }

    time_t now = time(NULL);

    for (int i = 0; i < CACHE_SIZE; ++i) {
        if (dns_cache[i].hostname != NULL && strcmp(dns_cache[i].hostname, hostname) == 0) {
            if (now - dns_cache[i].last_used > 300) {
                break;
            }

            dns_cache[i].last_used = now;
            return dns_cache[i].ip_address;
        }
    }

    // append _minecraft._tcp. to the hostname
    char srv_hostname[256];
    srv_hostname[0] = '\0';
    strncat(srv_hostname, "_minecraft._tcp.", 255);
    strncat(srv_hostname, hostname, 255);

    char ip_address[16];

    if (dns_query(srv_hostname, ip_address) == -1) {
        return NULL;
    }

    size_t lru_index = 0;
    for (size_t i = 0; i < CACHE_SIZE; ++i) {
        if (dns_cache[i].hostname == NULL) {
            lru_index = i;
            break;
        } else if (dns_cache[i].last_used < dns_cache[lru_index].last_used) {
            lru_index = i;
        }
    }

    free(dns_cache[lru_index].hostname);
    free(dns_cache[lru_index].ip_address);
    dns_cache[lru_index].hostname = strdup(hostname);
    dns_cache[lru_index].ip_address = strdup(ip_address);
    dns_cache[lru_index].last_used = time(NULL);

    return strdup(ip_address);
}

int parseVarInt(char* buffer, size_t* cursor) {
    size_t position = 0;
    int value = 0;

    while (1) {
        size_t byte = buffer[(*cursor)++];

        value |= (byte & 0x7F) << (position);

        if ((byte & 0x80) != 0x80) {
            break;
        }

        position += 7;

        if (position > 32) {
            return -1;
        } 
    }
    
    return value;
}

void parseHandshakePacket(char* buffer, char* server_address) {
    size_t cursor = 0;
    size_t packet_length = parseVarInt(buffer, &cursor);
    
    if (cursor < 1) {
        return;
    }

    char packet_id = buffer[cursor++];

    if (packet_id != 0x0) {
        return;
    }

    int version = parseVarInt(buffer, &cursor);

    size_t address_length = parseVarInt(buffer, &cursor);

    if (address_length >= 256) {
        return;
    }

    memcpy(server_address, buffer + cursor, address_length);
    server_address[address_length] = '\0';
}

int parseLoginPacket(char* buffer, char* username) {
    size_t payload_len = buffer[0];
    int state = buffer[payload_len];

    if (state == STATE_LOGIN) {
        size_t username_len = buffer[payload_len + 3];

        if (username_len >= 32) {
            return -1;
        }

        memcpy(username, &buffer[payload_len + 4], username_len);
        username[username_len] = '\0';
        return 1;
    }

    return 0;
}
