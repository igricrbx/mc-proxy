#ifndef PACKET_TOOLS_H
#define PACKET_TOOLS_H

#include <time.h>

#define CACHE_SIZE 16

struct CacheEntry {
    char* hostname;
    char* ip_address;
    time_t last_used;
};

int parseVarInt(char* buffer, int* cursor);
void parseHandshakePacket(char* buffer, char* server_address);
int parseLoginPacket(char* buffer, char* username);
char* resolve_hostname(const char* hostname);

#endif // PACKET_TOOLS_H