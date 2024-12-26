#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

enum LogConnectionType
{
    LOG_CONNECTED,
    LOG_DISCONNECTED
};

ssize_t log_init();
void log_shutdown();
void log_connection(const char *username, const char *client_ip, const char *server_ip_address, const char *resolved_address, enum LogConnectionType connection_type);
void log_error(const char* const message);

#endif // LOGGER_H