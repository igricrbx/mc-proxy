#include "logger.h"

#define DATE_FORMAT "%Y-%m-%d"
#define TIME_FORMAT "%Y-%m-%dT%H:%M:%S%z"
#define FILENAME_FORMAT "logs/%s.txt"

// Read/Write
#define MKDIR_MODE 0600 

pthread_mutex_t logger_mutex;

char log_file[256];
int log_file_day = 0;

ssize_t log_refresh_file() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    log_file_day = tm->tm_mday;

    char date_str[11];
    strftime(date_str, sizeof(date_str), DATE_FORMAT, tm);
    snprintf(log_file, sizeof(log_file), FILENAME_FORMAT, date_str);

    struct stat st = {0};
    if (stat("logs", &st) == -1) {
        return mkdir("logs", MKDIR_MODE);
    }
    return 0;
}

void log_message(const char *message) {
    pthread_mutex_lock(&logger_mutex);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if (tm->tm_mday != log_file_day) {
        if (log_refresh_file() < 0) {
            perror("Error creating log file");
            pthread_mutex_unlock(&logger_mutex);
            return;
        }
    }

    FILE* file = fopen(log_file, "a");

    if (file == NULL) {
        perror("Error opening log file");
        pthread_mutex_unlock(&logger_mutex);
        return;
    }

    char time_str[30];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S%z", tm); // RFC 5424 :3

    fprintf(file, "%s: %s\n", time_str, message);
    fclose(file);

    pthread_mutex_unlock(&logger_mutex);
}

ssize_t log_init() {
    if (log_refresh_file() < 0) {
        perror("Error creating log file");
        return -1;
    }
    if (pthread_mutex_init(&logger_mutex, NULL) != 0) {
        perror("Error creating logger mutex");
        return -1;
    }
    log_message("Server initialized");
    return 0;
}

void log_shutdown() {
    log_message("Shutting down the server");
}


void log_error(const char* const message) {
    char error_message[512];
    sprintf(error_message, "[ERROR]: %s", message);
    log_message(error_message);
}


void log_connection(const char *username, const char *client_ip, const char *server_ip_address, const char *resolved_address, enum LogConnectionType connection_type) {
    char message[512];
    if (connection_type == LOG_CONNECTED) {
        sprintf(message, "Client %s (%s) connected to %s (%s)", username, client_ip, server_ip_address, resolved_address);
    } else {
        sprintf(message, "Client %s (%s) disconnected from %s (%s)", username, client_ip, server_ip_address, resolved_address);
    }

    log_message(message);
}