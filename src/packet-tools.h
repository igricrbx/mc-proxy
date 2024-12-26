#ifndef PACKET_TOOLS_H
#define PACKET_TOOLS_H

#include <string.h>
#include <sys/types.h>

ssize_t parseVarInt(char* buffer, ssize_t* cursor);
void parseHandshakePacket(char* buffer, char* server_address);
int parseLoginPacket(char* buffer, char* username);

#endif // PACKET_TOOLS_H