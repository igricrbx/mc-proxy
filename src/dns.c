#include "dns.h"

#define MC_SRV_PREFIX "_minecraft._tcp."

pthread_mutex_t dns_cache_mutex;

CacheEntry* dns_cache;
size_t CACHE_SIZE = 0;

ssize_t dns_cache_init(size_t cache_size) {
    dns_cache = malloc(cache_size * sizeof(CacheEntry));
    if (dns_cache == NULL) {
        return -1;
    }

    if (pthread_mutex_init(&dns_cache_mutex, NULL) != 0) {
        free(dns_cache);
        dns_cache = NULL;
        return -1;
    }

    for (size_t i = 0; i < cache_size; ++i) {
        dns_cache[i].hostname[0] = '\0';
        dns_cache[i].ip_address[0] = '\0';
        dns_cache[i].last_used = 0;
    }

    CACHE_SIZE = cache_size;
    return 0;
}

void dns_cache_destroy() {
    CACHE_SIZE = 0;
    free(dns_cache);
    dns_cache = NULL;
}

ssize_t resolve_hostname(const char* const hostname, char* const address) {
    struct sockaddr_in sa;

    if (dns_cache == NULL) {
        printf("DNS cache not initialized\n");
        return -1;
    }

    int result = inet_pton(AF_INET, hostname, &(sa.sin_addr));
    // if the hostname is already an IP address, no need to query the DNS
    if (result != 0)
    {
        strncpy(address, hostname, 15);
        return 0;
    }

    time_t now = time(NULL);

    pthread_mutex_lock(&dns_cache_mutex);

    for (size_t i = 0; i < CACHE_SIZE; ++i) {
        if (dns_cache[i].hostname != NULL && strcmp(dns_cache[i].hostname, hostname) == 0) {
            if (now - dns_cache[i].last_used > 300) {
                break; // if the entry is older than 5 minutes, re-query the DNS
            }

            dns_cache[i].last_used = now;
            strncpy(address, dns_cache[i].ip_address, 15);
            pthread_mutex_unlock(&dns_cache_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&dns_cache_mutex);

    char ip_address[16];

    if (dns_query_mc(hostname, ip_address) < 0) {
        return -1;
    }

    pthread_mutex_lock(&dns_cache_mutex);

    ssize_t lru_index = -1;
    for (size_t i = 0; i < CACHE_SIZE; ++i) {
        if (dns_cache[i].hostname[0] == '\0') {
            lru_index = i;
            break;
        }
    }

    if (lru_index == -1) {
        time_t lru_time = now;
        for (size_t i = 0; i < CACHE_SIZE; ++i) {
            if (dns_cache[i].last_used <= lru_time) {
                lru_time = dns_cache[i].last_used;
                lru_index = i;
            }
        }
    }
    
    strncpy(dns_cache[lru_index].hostname, hostname, 255);
    strncpy(dns_cache[lru_index].ip_address, ip_address, 15);
    dns_cache[lru_index].last_used = now;

    pthread_mutex_unlock(&dns_cache_mutex);

    strncpy(address, ip_address, 15);
    return 0;
}

void parse_dns_response(const char* response, char* domain, const size_t max_len) {
    const char* response_ptr = response;
    char* domain_ptr = domain;
    size_t total_len = 0;

    while (*response_ptr != '\0' && total_len < max_len - 1) {
        size_t len = *response_ptr++;
        if (len <= 0) {
            break;
        }

        if (total_len + len >= max_len - 1) {
            // If the length of the label exceeds the remaining space in the domain buffer, truncate
            len = max_len - 1 - total_len;
        }

        strncpy(domain_ptr, response_ptr, len);
        domain_ptr += len;
        response_ptr += len;
        total_len += len;

        if (total_len < max_len - 1) {
            *domain_ptr++ = '.';
            total_len++;
        }
    }

    // Ensure the domain is null-terminated
    if (total_len > 0 && *(domain_ptr - 1) == '.') {
        *(domain_ptr - 1) = '\0';
    } else {
        *domain_ptr = '\0';
    }
}

ns_type get_dns_record(const char* const fqdn, char* address, const ns_type query_type) {
    char response[NS_PACKETSZ];
    char domain_name[256];
    strncpy(domain_name, fqdn, 255);

    ssize_t len = res_query((const char *)domain_name, ns_c_in, query_type, response, sizeof(response));

    if (len < 0) {
        address = NULL;
        return ns_t_invalid;
    }

    ns_msg msg;

    if (ns_initparse(response, len, &msg) < 0)
    {
        printf("Parsing failed\n");
        address = NULL;
        return ns_t_invalid;
    }

    for (ssize_t i = ns_msg_count(msg, ns_s_an) - 1; i >= 0; i--)
    {
        ns_rr rr;
        char target[256];
        
        if (ns_parserr(&msg, ns_s_an, i, &rr))
        {
            printf("Error parsing record\n");
            continue;
        }

        switch(ns_rr_type(rr))
        {
            case ns_t_srv:
                //TODO: consider using the port given in the SRV record (ns_get16(ns_rr_rdata(rr) + 4))
                parse_dns_response(ns_rr_rdata(rr) + 6, target, sizeof(target));
                strncpy(address, target, 255);
                return ns_rr_type(rr);
            case ns_t_a:
                snprintf(target, 16, "%d.%d.%d.%d", 
                    ns_rr_rdata(rr)[0], 
                    ns_rr_rdata(rr)[1], 
                    ns_rr_rdata(rr)[2], 
                    ns_rr_rdata(rr)[3]
                );
                strncpy(address, target, 15);
                return ns_rr_type(rr);
            case ns_t_cname:
                parse_dns_response(ns_rr_rdata(rr), target, sizeof(target));
                strncpy(address, target, 255);
                return ns_rr_type(rr);
        }
    }
}

ssize_t dns_query_mc(const char* const fqdn, char* const address)
{
    char domain_name[256];
    char query_fqdn[256];

    // Check if the fqdn is a SRV record
    // Append _minecraft._tcp. to the fqdn
    snprintf(query_fqdn, 255, "%s%s", MC_SRV_PREFIX, fqdn);
    ns_type record_type = get_dns_record(query_fqdn, domain_name, ns_t_srv);
    if (record_type == ns_t_srv)
    {
        strncpy(query_fqdn, domain_name, 255);
    } else {
        strncpy(query_fqdn, fqdn, 255);
    }

    // If the fqdn is not a SRV record, check if it is an A record
    record_type = get_dns_record(query_fqdn, domain_name, ns_t_a);
    if (record_type == ns_t_a)
    {
        strncpy(address, domain_name, 15);
        return 0;
    } else if (record_type == ns_t_cname) {
        printf("TODO: add support for CNAME records\n");
    } else {
        printf("DNS query error\n");
        return -1;
    }
    
    return -1;
}
