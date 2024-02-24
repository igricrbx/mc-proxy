#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "logger.h"

#define DATE_FORMAT "%Y-%m-%d"
#define TIME_FORMAT "%Y-%m-%dT%H:%M:%S%z"
#define FILENAME_FORMAT "logs/%s.txt"

// Read/Write
#define MKDIR_MODE 0600 

pthread_mutex_t loggerMutex;

char log_file[256];
int log_file_day = 0;

void log_refresh_file() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    log_file_day = tm->tm_mday;

    char date_str[11];
    strftime(date_str, sizeof(date_str), DATE_FORMAT, tm);
    snprintf(log_file, sizeof(log_file), FILENAME_FORMAT, date_str);

    struct stat st = {0};
    if (stat("logs", &st) == -1) {
        mkdir("logs", MKDIR_MODE);
    }
}

void log_message(const char *message) {
    pthread_mutex_lock(&loggerMutex);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm->tm_mday != log_file_day) {
        log_refresh_file();
    }

    FILE* file = fopen(log_file, "a");

    if (file == NULL) {
        perror("Error opening log file");
        pthread_mutex_unlock(&loggerMutex);
        return;
    }

    char time_str[30];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S%z", tm); // RFC 5424 :3

    fprintf(file, "%s: %s\n", time_str, message);
    fclose(file);

    pthread_mutex_unlock(&loggerMutex);
}

void log_init() {
    log_refresh_file();
    log_message("Server initialized");
}

void log_shutdown() {
    log_message("Shutting down the server");
}


void log_error(const char *message) {
    char error_message[256];
    sprintf(error_message, "[ERROR]: %s", message);
    log_message(error_message);
}


void log_connection(const char *username, const char *client_ip, const char *server_ip_address, const char *resolved_address, enum LogConnectionType connection_type) {
    char message[256];
    if (connection_type == LOG_CONNECTED) {
        sprintf(message, "Client %s (%s) connected to %s (%s)", username, client_ip, server_ip_address, resolved_address);
    } else {
        sprintf(message, "Client %s (%s) disconnected from %s (%s)", username, client_ip, server_ip_address, resolved_address);
    }

    log_message(message);
}