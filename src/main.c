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

// In case of an error, log the error and exit the program
void handle_error(const char* error_message) {
    perror(error_message);
    log_error(error_message);
    exit(EXIT_FAILURE);
}

// Initialize the mutexes
void mutex_init() {
    pthread_mutex_init(&loggerMutex, NULL);
    pthread_mutex_init(&serversMutex, NULL);
}

// Destroy the mutexes
void mutex_destroy() {
    pthread_mutex_destroy(&loggerMutex);
    pthread_mutex_destroy(&serversMutex);
}

void log_connect(char *username, int client_socket, char* server_ip_address, char* resolved_address, enum LogConnectionType connection_type) {
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    // Get the client's IP address
    if (getpeername(client_socket, (struct sockaddr *)&client_address, &client_address_length) == -1) {
        perror("getpeername failed");
        return;
    }

    // Convert the client's IP address to a string
    char *client_ip = inet_ntoa(client_address.sin_addr);

    // Log the connection
    log_connection(username, client_ip, server_ip_address, resolved_address, connection_type);
}


int create_and_bind_socket() {
    // Create the server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        handle_error("Error creating socket");
    }

    // Allow the server to reuse the address, this makes sure the server can be restarted without waiting for the socket to be released
    int enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        handle_error("setsockopt(SO_REUSEADDR) failed");
    }

    // Set up the address struct for the server socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    // Bind the socket to the address and port
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        handle_error("Error binding socket");
    }

    return server_socket;
}

int create_and_connect_socket(char* address, short port) {
    // Create the socket for the destination server
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating to socket");
        return -1;
    }

    // Set up the address struct for the destination server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    // Convert the address to a binary form
    if (inet_pton(AF_INET, address, &(server_address.sin_addr)) <= 0) {
        perror("inet_pton error");
        return -1;
    }

    // Attempt to connect to the destination server
    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to socket");
        return -1;
    }

    return server_socket;
}

void listen_and_accept_connections(int server_socket) {
    // Start listening to the server socket
    if (listen(server_socket, MAX_PENDING_CONNECTIONS) == -1) {
        handle_error("Error listening to the socket");
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // Accept all incoming connections
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);
        // Accept the client's connection
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        // Create a new thread for each client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, &client_socket) != 0) {
            perror("Error creating thread");
            continue;
        }

        // Detach the thread so it can be cleaned up automatically
        pthread_detach(client_thread);
    }
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[BUFFER_SIZE];
    char server_ip_address[256] = {0};

    // Receive the packet from the client
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

    // Connection closed by the client
    if (bytes_received <= 0) {
        return NULL;
    }
    
    // Assume the packet is a handshake packet
    parseHandshakePacket(buffer, server_ip_address);

    // Not a valid packet
    if (strlen(server_ip_address) == 0) {
        return NULL;
    }

    // Find the server in the dictionary
    Entry* entry = find_entry(server_ip_address);

    // Exit if the target server is not found
    if (entry == NULL) {
        printf("Server not found (%s)\n", server_ip_address);
        return NULL;
    }

    // Resolve the hostname to an IP address TODO: make thread-safe
    char* resolved_address = resolve_hostname(entry->destination);

    // Exit if the hostname could not be resolved
    if (resolved_address == NULL) {
        printf("Could not resolve the hostname\n");
        return NULL;
    }

    // Attempt to connect to the destination server
    int server_socket = create_and_connect_socket(resolved_address, entry->port);

    // Exit if the connection was refused
    if (server_socket == -1) {
        printf("Connection refused\n");
        return NULL;
    }

    char username[32];

    // Check if the packet is a login packet, meaning the player is attempting to join the server
    int is_login_packet = parseLoginPacket(buffer, username);

    // Log the connection if the player is trying to join the server
    if (is_login_packet) {
        log_connect(username, client_socket, server_ip_address, resolved_address, LOG_CONNECTED);
    }

    // Forward the packet to the server
    if (write(server_socket, buffer, bytes_received) <= 0) {
        perror("Error responding to the server");
        close(server_socket);
        return NULL;
    }

    // Establish a connection between the client and the server
    fd_set set;
    while (1) {
        FD_ZERO(&set);
        FD_SET(client_socket, &set);
        FD_SET(server_socket, &set);

        // Wait for either the client or the server to send a packet
        if (select(FD_SETSIZE, &set, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // Forward the packet from the client to the server
        if (FD_ISSET(client_socket, &set)) {
            int bytes = read(client_socket, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            if (write(server_socket, buffer, bytes) <= 0) break;
        }

        // Forward the packet from the server to the client
        if (FD_ISSET(server_socket, &set)) {
            int bytes = read(server_socket, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            if (write(client_socket, buffer, bytes) <= 0) break;
        }
    }

    // Log the disconnection if the player was connected to the server
    if (is_login_packet) {
        log_connect(username, client_socket, server_ip_address, resolved_address, LOG_DISCONNECTED);
    }

    close(server_socket);

    return NULL;
}

int server_socket = 0;

void sigint_handler(int) {
    printf("\nShutting down the server proxy\n");
    // Log the server shutdown
    log_shutdown();

    // Close the server socket
    if (server_socket) close(server_socket);
    mutex_destroy();

    // Exit the program
    exit(EXIT_SUCCESS);
}

int main() {
    printf("The server proxy is starting\n");

    // Initialize mutexes and logging mechanisms
    mutex_init();
    log_init();

    // Loads the servers from the config file
    load_dictionary("servers.conf");

    server_socket = create_and_bind_socket();

    // Handle SIGINT and shutdown gracefully
    signal(SIGINT, sigint_handler);

    // Start listening to the server socket
    listen_and_accept_connections(server_socket);

    close(server_socket);

    mutex_destroy();

    return EXIT_FAILURE;
}
