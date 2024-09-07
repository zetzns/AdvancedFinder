#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

bool is_valid_ipv6(const char *addr) {
    struct in6_addr ipv6_addr;
    return inet_pton(AF_INET6, addr, &ipv6_addr) == 1;
}

bool search_ipv6_address_in_line(char *line, const char *ipv6_address) {
    char *word = strtok(line, " ,;[](){}<>\"\'\\|\t");
    while (word != NULL) {
        if (is_valid_ipv6(word) && strcmp(word, ipv6_address) == 0) {
            return true;
        }
        word = strtok(NULL, " ,;[](){}<>\"\'\\|\t");
    }
    return false;
}

int plugin_process_file(const char* fname, struct option in_opts[], size_t in_opts_len) {
    if (getenv("DEBUG")) {
        fprintf(stderr, "plugin_process_file: fname=%s, option=%s, in_opts_len=%zu\n", fname, in_opts[0].name, in_opts_len);
    }

    if (in_opts_len != 1 || strcmp(in_opts[0].name, "ipv6-addr") != 0) {
        fprintf(stderr, "Invalid option for this plugin\n");
        return -2;
    }

    const char *ipv6_address = (const char *)in_opts[0].flag;
    if (getenv("DEBUG")) {
        fprintf(stderr, "plugin_process_file: ipv6_address=%s\n", ipv6_address);
    }

    if (!is_valid_ipv6(ipv6_address)) {
        fprintf(stderr, "Invalid IPv6 address: %s\n", ipv6_address);
        return -1;
    }

    FILE *file = fopen(fname, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    if (getenv("DEBUG")) {
        fprintf(stderr, "Opened file: %s\n", fname);
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (getenv("DEBUG")) {
            fprintf(stderr, "Read line: %s\n", line);
        }
        char line_copy[MAX_LINE_LENGTH];
        strncpy(line_copy, line, MAX_LINE_LENGTH - 1);
        line_copy[MAX_LINE_LENGTH - 1] = '\0';
        if (search_ipv6_address_in_line(line_copy, ipv6_address)) {
            if (getenv("DEBUG")) {
                fprintf(stderr, "Found IPv6 address in line: %s\n", line);
            }
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return 1;
}

struct plugin_option plugin_options[] = {
    {{"ipv6-addr", required_argument, NULL, 0}, "Search for files containing the specified IPv6 address"}
};

struct plugin_info plugin_info = {
    "Search for files containing a specific IPv6 address",
    "Alexandr Zakurin",
    sizeof(plugin_options) / sizeof(plugin_options[0]),
    plugin_options
};

int plugin_get_info(struct plugin_info* ppi) {
    if (!ppi) return -1;
    ppi->plugin_purpose = plugin_info.plugin_purpose;
    ppi->plugin_author = plugin_info.plugin_author;
    ppi->sup_opts_len = plugin_info.sup_opts_len;
    ppi->sup_opts = plugin_info.sup_opts;
    return 0;
}
