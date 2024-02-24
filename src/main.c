#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <netdb.h>
#include <signal.h>

#include "servers.h"
#include "packet-tools.h"
#include "logger.h"

#define SERVER_PORT 25565
#define MAX_PENDING_CONNECTIONS 256

#define BUFFER_SIZE 32768 // MC packets don't exceed 32768 (2^15) bytes.

void *handle_client(void *arg);

void handle_error(const char* error_message) {
    perror(error_message);
    log_error(error_message);
    exit(EXIT_FAILURE);
}

void mutex_init() {
    pthread_mutex_init(&loggerMutex, NULL);
    pthread_mutex_init(&serversMutex, NULL);
}

void mutex_destroy() {
    pthread_mutex_destroy(&loggerMutex);
    pthread_mutex_destroy(&serversMutex);
}

void log_connect(char *username, int client_socket, char* server_ip_address, char* resolved_address, enum LogConnectionType connection_type) {
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    if (getpeername(client_socket, (struct sockaddr *)&client_address, &client_address_length) == -1) {
        perror("getpeername failed");
        return;
    }

    char *client_ip = inet_ntoa(client_address.sin_addr);

    log_connection(username, client_ip, server_ip_address, resolved_address, connection_type);
}


int create_and_bind_socket() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        handle_error("Error creating socket");
    }

    int enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        handle_error("setsockopt(SO_REUSEADDR) failed");
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        handle_error("Error binding socket");
    }

    return server_socket;
}

int create_and_connect_socket(char* address, short port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating to socket");
        return -1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, address, &(server_address.sin_addr)) <= 0) {
        perror("inet_pton error");
        return -1;
    }

    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to socket");
        return -1;
    }

    return server_socket;
}

void listen_and_accept_connections(int server_socket) {
    if (listen(server_socket, MAX_PENDING_CONNECTIONS) == -1) {
        handle_error("Error listening to the socket");
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, &client_socket) != 0) {
            perror("Error creating thread");
            continue;
        }

        pthread_detach(client_thread);
    }
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    char server_ip_address[256] = {0};

    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes_received <= 0) {
        return NULL;
    }
    
    parseHandshakePacket(buffer, server_ip_address);

    // Not a valid packet
    if (strlen(server_ip_address) == 0) {
        return NULL;
    }

    Entry* entry = find_entry(server_ip_address);

    if (entry == NULL) {
        printf("Server not found (%s)\n", server_ip_address);
        return NULL;
    }

    char* resolved_address = resolve_hostname(entry->destination);

    if (resolved_address == NULL) {
        printf("Could not resolve the hostname\n");
        return NULL;
    }

    int server_socket = create_and_connect_socket(resolved_address, entry->port);

    if (server_socket == -1) {
        printf("Connection refused\n");
        return NULL;
    }

    char username[32];

    int is_login_packet = parseLoginPacket(buffer, username);

    // Log the connection if the player is trying to join the server
    if (is_login_packet) {
        log_connect(username, client_socket, server_ip_address, resolved_address, LOG_CONNECTED);
    }

    if (write(server_socket, buffer, bytes_received) <= 0) {
        perror("Error responding to the server");
        close(server_socket);
        return NULL;
    }

    fd_set set;
    while (1) {
        FD_ZERO(&set);
        FD_SET(client_socket, &set);
        FD_SET(server_socket, &set);

        if (select(FD_SETSIZE, &set, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(client_socket, &set)) {
            int bytes = read(client_socket, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            if (write(server_socket, buffer, bytes) <= 0) break;
        }

        if (FD_ISSET(server_socket, &set)) {
            int bytes = read(server_socket, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            if (write(client_socket, buffer, bytes) <= 0) break;
        }
    }

    if (is_login_packet) {
        log_connect(username, client_socket, server_ip_address, resolved_address, LOG_DISCONNECTED);
    }

    close(server_socket);

    return NULL;
}

int server_socket;

void sigint_handler(int) {
    printf("\nShutting down the server proxy\n");
    log_shutdown();

    if (server_socket) close(server_socket);
    mutex_destroy();

    exit(0);
}

int main() {
    printf("The server proxy is starting\n");

    mutex_init();

    log_init();
    load_dictionary("servers.conf");

    server_socket = create_and_bind_socket();

    signal(SIGINT, sigint_handler);

    listen_and_accept_connections(server_socket);

    close(server_socket);

    mutex_destroy();

    return 0;
}
