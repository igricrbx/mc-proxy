#include <stdio.h>
#include <netinet/in.h>
#include <resolv.h>
#include <arpa/nameser.h>
#include <string.h>

#include "dns.h"

void parse_dns_response(const unsigned char *response, char *domain, int max_len)
{
    int i = 0;
    int j = 0;
    while (response[i] != 0)
    {
        int len = response[i++];

        for (int end = i + len; i < end; i++, j++)
        {
            if (j < max_len - 1)
            {
                domain[j] = response[i];
            }
            else
            {
                // The domain string is not large enough to hold the entire domain name
                domain[j] = '\0';
                return;
            }
        }

        // Add a dot after the label, unless this is the last label
        if (response[i] != 0 && j < max_len - 1)
        {
            domain[j++] = '.';
        }
    }

    domain[j] = '\0';
}

int dns_query(char *fqdn, char *address)
{
    unsigned char response[NS_PACKETSZ]; // Buffer for the DNS response
    ns_msg msg;                          // Structure for the DNS message
    ns_rr rr;                            // Structure for the DNS resource record
    int len;

    int depth = 0;

    char domain_name[256];
    strcpy(domain_name, fqdn);

    if (!((len = res_query((const char *)domain_name, ns_c_in, ns_t_srv, response, sizeof(response))) < 0))
    {
        // Parse the DNS message
        if (ns_initparse(response, len, &msg) < 0)
        {
            printf("Parsing failed\n");
            address = NULL;
            return -1;
        }

        for (int i = ns_msg_count(msg, ns_s_an) - 1; i >= 0; i--)
        {
            if (ns_parserr(&msg, ns_s_an, i, &rr))
            {
                printf("Error parsing record\n");
                continue;
            }
            if (ns_rr_type(rr) == ns_t_srv)
            {
                //printf("Priority: %u\n", ns_get16(ns_rr_rdata(rr)));
                //printf("Weight: %u\n", ns_get16(ns_rr_rdata(rr) + 2));
                //printf("Port: %u\n", ns_get16(ns_rr_rdata(rr) + 4));

                char target[256];
                parse_dns_response(ns_rr_rdata(rr) + 6, target, sizeof(target));
                strcpy(domain_name, target);
                break;
            }
        }
    }

    while (depth < DNS_MAX_DEPTH) {
        if ((len = res_query(domain_name, ns_c_in, ns_t_a, response, sizeof(response))) < 0)
        {
            printf("Query failed\n");
            address = NULL;
            return -1;
        }

        if (ns_initparse(response, len, &msg) < 0)
        {
            printf("Parsing failed\n");
            return 1;
        }
        
        for (int i = ns_msg_count(msg, ns_s_an) - 1; i >= 0; i--)
        {
            if (ns_parserr(&msg, ns_s_an, i, &rr))
            {
                printf("Error parsing record\n");
                continue;
            }
            if (ns_rr_type(rr) == ns_t_a)
            {
                sprintf(address, "%d.%d.%d.%d", 
                    ns_rr_rdata(rr)[0], 
                    ns_rr_rdata(rr)[1], 
                    ns_rr_rdata(rr)[2], 
                    ns_rr_rdata(rr)[3]
                );
                return 0;
            } else if (ns_rr_type(rr) == ns_t_cname)
            {
                char domain[256];
                parse_dns_response(ns_rr_rdata(rr), domain, sizeof(domain));
                strcpy(domain_name, domain);
                ++depth;
                break;
            }        
        }
    }

    address = NULL;
    return -1;
}