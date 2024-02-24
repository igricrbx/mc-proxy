#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>

extern pthread_mutex_t loggerMutex;

enum LogConnectionType
{
    LOG_CONNECTED,
    LOG_DISCONNECTED
};

void log_init();
void log_shutdown();
void log_connection(const char *username, const char *client_ip, const char *server_ip_address, const char *resolved_address, enum LogConnectionType connection_type);
void log_error(const char *message);


#endif