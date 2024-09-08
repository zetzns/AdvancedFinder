#ifndef PLUGIN_H
#define PLUGIN_H

#include <stdbool.h>
#include <getopt.h>
#include <stddef.h>

struct plugin_option {
    struct option opt;
    const char *opt_descr;
};

struct plugin_info {
    const char *plugin_purpose;
    const char *plugin_author;
    size_t sup_opts_len;
    struct plugin_option *sup_opts;
};

int plugin_get_info(struct plugin_info* ppi);
int plugin_process_file(const char* fname, struct option in_opts[], size_t in_opts_len);
bool is_valid_ipv6(const char* str);
bool search_ipv6_address_in_line(char* line, const char* ipv6_address);

#endif // PLUGIN_H
