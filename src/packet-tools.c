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
    strcpy(srv_hostname, "_minecraft._tcp.");
    strcat(srv_hostname, hostname);

    char ip_address[16];

    if (dns_query(srv_hostname, ip_address) == -1) {
        return NULL;
    }

    int lru_index = 0;
    for (int i = 0; i < CACHE_SIZE; ++i) {
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

int parseVarInt(char* buffer, int* cursor) {
    int position = 0;
    int value = 0;

    while (1) {
        unsigned int byte = buffer[(*cursor)++];

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
    int cursor = 0;
    int packet_length = parseVarInt(buffer, &cursor);
    
    if (cursor < 1) {
        return;
    }

    char packet_id = buffer[cursor++];

    if (packet_id != 0x0) {
        return;
    }

    int version = parseVarInt(buffer, &cursor);

    int address_length = parseVarInt(buffer, &cursor);

    memcpy(server_address, buffer + cursor, address_length);
    server_address[address_length] = '\0';
}

int parseLoginPacket(char* buffer, char* username) {
    int payload_len = buffer[0];
    int state = buffer[payload_len];

    if (state == STATE_LOGIN) {
        int username_len = buffer[payload_len + 3];
        memcpy(username, &buffer[payload_len + 4], username_len);
        username[username_len] = '\0';
        return 1;
    }

    return 0;
}