#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <getopt.h>
#include "plugin.h"

#define MAX_PARAMS 10
#define MAX_PLUGINS 10
#define PATH_MAX 4096

typedef struct {
    void* handle;
    int (*plugin_get_info)(struct plugin_info* ppi);
    int (*plugin_process_file)(const char* fname, struct option in_opts[], size_t in_opts_len);
} Plugin;

typedef struct {
    char* name;
    char* value;
} Param;

Plugin plugins[MAX_PLUGINS];
Param params[MAX_PARAMS];
int plugin_count = 0;
int param_count = 0;

int opt_and = 0;
int opt_or = 1;
int opt_negate = 0;

void parse_param(const char* param, const char* value) {
    if (param_count >= MAX_PARAMS) {
        fprintf(stderr, "Exceeded maximum number of parameters\n");
        return;
    }
    params[param_count].name = strdup(param);
    params[param_count].value = value ? strdup(value) : NULL;
    param_count++;
    if (getenv("DEBUG")) {
        fprintf(stderr, "Parsed param: %s, value: %s\n", param, value);
    }
}

char* find_param_value(const char* name) {
    if (getenv("DEBUG")) {
        fprintf(stderr, "Searching for param value: %s\n", name);
    }
    for (int i = 0; i < param_count; i++) {
        if (getenv("DEBUG")) {
            fprintf(stderr, "Checking param: %s, value: %s\n", params[i].name, params[i].value);
        }
        if (strcmp(params[i].name, name) == 0) {
            return params[i].value;
        }
    }
    return NULL;
}

void load_plugins(const char* directory) {
    DIR* dir = opendir(directory);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open plugin directory: %s\n", directory);
        return;
    }

    struct dirent* entry;
    char path[PATH_MAX];
    while ((entry = readdir(dir)) != NULL && plugin_count < MAX_PLUGINS) {
        if (strstr(entry->d_name, ".so")) {
            snprintf(path, PATH_MAX, "%s/%s", directory, entry->d_name);
            void* handle = dlopen(path, RTLD_NOW);
            if (handle == NULL) {
                fprintf(stderr, "Failed to load plugin %s: %s\n", path, dlerror());
                continue;
            }

            int (*plugin_get_info)(struct plugin_info* ppi) = dlsym(handle, "plugin_get_info");
            int (*plugin_process_file)(const char* fname, struct option in_opts[], size_t in_opts_len) = dlsym(handle, "plugin_process_file");

            if (plugin_get_info == NULL || plugin_process_file == NULL) {
                fprintf(stderr, "Failed to load functions from plugin %s: %s\n", path, dlerror());
                dlclose(handle);
                continue;
            }

            struct plugin_info ppi;
            if (plugin_get_info(&ppi) < 0) {
                fprintf(stderr, "Failed to get plugin info from %s\n", path);
                dlclose(handle);
                continue;
            }

            plugins[plugin_count].handle = handle;
            plugins[plugin_count].plugin_get_info = plugin_get_info;
            plugins[plugin_count].plugin_process_file = plugin_process_file;
            plugin_count++;

            if (getenv("DEBUG")) {
                fprintf(stderr, "Loaded plugin: %s, purpose: %s, author: %s\n", path, ppi.plugin_purpose, ppi.plugin_author);
            }
        }
    }

    closedir(dir);
}

void unload_plugins() {
    for (int i = 0; i < plugin_count; i++) {
        dlclose(plugins[i].handle);
    }
}

void process_file_with_plugins(const char* filename) {
    int final_result = opt_and ? 1 : 0;

    for (int i = 0; i < plugin_count; i++) {
        struct plugin_info ppi;
        plugins[i].plugin_get_info(&ppi);

        int plugin_used = 0;

        struct option* in_opts = (struct option*)calloc(ppi.sup_opts_len, sizeof(struct option));
        char** values = (char**)calloc(ppi.sup_opts_len, sizeof(char*));
        size_t opts_used = 0;

        for (size_t j = 0; j < ppi.sup_opts_len; j++) {
            const char* plugin_option = ppi.sup_opts[j].opt.name;

            char param_name[64];
            snprintf(param_name, sizeof(param_name), "--%s", plugin_option);

            char* param_value = find_param_value(param_name);
            if (param_value == NULL) {
                if (getenv("DEBUG")) {
                    fprintf(stderr, "Option %s not provided, skipping plugin\n", plugin_option);
                }
                continue;
            }

            plugin_used = 1;
            in_opts[opts_used] = ppi.sup_opts[j].opt;
            values[opts_used] = param_value;
            opts_used++;

            if (getenv("DEBUG")) {
                fprintf(stderr, "Processing file: %s with plugin option: %s and value: %s\n", filename, plugin_option, param_value);
            }
        }

        if (plugin_used) {
            for (size_t j = 0; j < opts_used; j++) {
                in_opts[j].flag = (int*)values[j];
            }

            if (getenv("DEBUG")) {
                fprintf(stderr, "Calling plugin_process_file with: fname=%s, opts_used=%zu\n", filename, opts_used);
            }

            int result = plugins[i].plugin_process_file(filename, in_opts, opts_used);
            if (getenv("DEBUG")) {
                fprintf(stderr, "Plugin processed file %s with result %d\n", filename, result);
            }

            if (result < 0) {
                final_result = 0;
                if (opt_and) break;
                else continue;
            }
            if (opt_and) {
                final_result &= (result == 0);
            } else if (opt_or) {
                final_result |= (result == 0);
            } else {
                final_result = (result == 0);
            }
        }

        if (!plugin_used && getenv("DEBUG")) {
            fprintf(stderr, "Plugin was not used for file %s\n", filename);
        }

        free(in_opts);
        free(values);
    }

    if (opt_negate) {
        final_result = !final_result;
    }

    if (final_result) {
        printf("File %s matches the search criteria\n", filename);
    } else if (getenv("DEBUG")) {
        fprintf(stderr, "File %s does not match the search criteria\n", filename);
    }
}

void process_directory(const char* directory) {
    DIR* dir = opendir(directory);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open directory: %s\n", directory);
        return;
    }

    struct dirent* entry;
    char path[PATH_MAX];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, PATH_MAX, "%s/%s", directory, entry->d_name);

        struct stat statbuf;
        if (stat(path, &statbuf) != 0) {
            fprintf(stderr, "Failed to get stat for %s\n", path);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            process_directory(path);
        } else if (S_ISREG(statbuf.st_mode)) {
            process_file_with_plugins(path);
        }
    }

    closedir(dir);
}

void process_params(int argc, char* argv[]) {
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        char* param = argv[i];
        char* value = (i + 1 < argc && argv[i + 1][0] != '-') ? argv[++i] : "";
        if (getenv("DEBUG")) {
            fprintf(stderr, "process_params: param=%s, value=%s\n", param, value);
        }
        parse_param(param, value);
        i++;
    }
}

void free_params() {
    for (int i = 0; i < param_count; i++) {
        free(params[i].name);
        free(params[i].value);
    }
}

int main(int argc, char* argv[]) {
    int opt;
    char *plugin_directory = "./plugins";
    int opt_index = 0;
    int long_index = 0;

    static struct option long_options[64];
    long_options[0] = (struct option){"plugin-dir", required_argument, 0, 'P'};
    long_options[1] = (struct option){"and", no_argument, 0, 'A'};
    long_options[2] = (struct option){"or", no_argument, 0, 'O'};
    long_options[3] = (struct option){"negate", no_argument, 0, 'N'};
    long_options[4] = (struct option){"help", no_argument, 0, 'h'};
    long_options[5] = (struct option){"version", no_argument, 0, 'v'};
    opt_index = 6;

    process_params(argc, argv);
    char* param_value = find_param_value("--plugin-dir");
    if (param_value != NULL) plugin_directory = param_value;

    load_plugins(plugin_directory);

    for (int i = 0; i < plugin_count; i++) {
        struct plugin_info ppi;
        plugins[i].plugin_get_info(&ppi);
        for (size_t j = 0; j < ppi.sup_opts_len; j++) {
            long_options[opt_index].name = strdup(ppi.sup_opts[j].opt.name);
            long_options[opt_index].has_arg = ppi.sup_opts[j].opt.has_arg;
            long_options[opt_index].flag = 0;
            long_options[opt_index].val = 0;
            opt_index++;
        }
    }

    long_options[opt_index] = (struct option){0, 0, 0, 0};

    optind = 1;

    while ((opt = getopt_long(argc, argv, "P:AONhv", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'P':
                plugin_directory = optarg;
                break;
            case 'A':
                opt_and = 1;
                opt_or = 0;
                break;
            case 'O':
                opt_and = 0;
                opt_or = 1;
                break;
            case 'N':
                opt_negate = 1;
                break;
            case 'h':
                printf("Usage: %s [-P <plugin_directory>] [-A] [-O] [-N] [-v] [-h] <directory>\n", argv[0]);
                free_params();
                for (int i = 6; i < opt_index; i++) {
                    free((char*)long_options[i].name);
                }
                return EXIT_SUCCESS;
            case 'v':
                printf("Версия единственная и неповторимая by Alexandr Zakurin\n");
                free_params();
                for (int i = 6; i < opt_index; i++) {
                    free((char*)long_options[i].name);
                }
                return EXIT_SUCCESS;
            case 0:
                break;
            default:
                fprintf(stderr, "Unknown option encountered\n");
                free_params();
                for (int i = 6; i < opt_index; i++) {
                    free((char*)long_options[i].name);
                }
                return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected directory after options\n");
        free_params();
        for (int i = 6; i < opt_index; i++) {
            free((char*)long_options[i].name);
        }
        return EXIT_FAILURE;
    }

    char *directory = argv[optind];

    struct stat statbuf;
    if (stat(directory, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr, "Provided path is not a directory: %s\n", directory);
        free_params();
        for (int i = 6; i < opt_index; i++) {
            free((char*)long_options[i].name);
        }
        return EXIT_FAILURE;
    }

    process_directory(directory);
    unload_plugins();

    free_params();
    for (int i = 6; i < opt_index; i++) {
        free((char*)long_options[i].name);
    }

    return EXIT_SUCCESS;
}

