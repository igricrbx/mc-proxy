#include "packet-tools.h"

#define STATE_LOGIN 0x02

ssize_t parseVarInt(char* buffer, ssize_t* cursor) {
    size_t position = 0;
    int value = 0;

    while (1) {
        char byte = buffer[(*cursor)++];

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
    ssize_t packet_length = parseVarInt(buffer, &cursor);
    
    if (cursor < 1) {
        return;
    }

    char packet_id = buffer[cursor++];

    if (packet_id != 0x0) {
        return;
    }

    int version = parseVarInt(buffer, &cursor);

    ssize_t address_length = parseVarInt(buffer, &cursor);

    if (address_length >= 256 || address_length < 1) {
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
