#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void apply_kv(cs_config *config, const char *key, const char *value) {
    if (strcmp(key, "width") == 0) {
        config->width = atoi(value);
    } else if (strcmp(key, "height") == 0) {
        config->height = atoi(value);
    } else if (strcmp(key, "fps") == 0) {
        config->fps = (float)atof(value);
    } else if (strcmp(key, "bitrate_kbps") == 0) {
        config->bitrate_kbps = atoi(value);
    } else if (strcmp(key, "signaling_port") == 0) {
        config->signaling_port = atoi(value);
    }
}

void cs_config_defaults(cs_config *config) {
    config->width = 640;
    config->height = 480;
    config->fps = 30.0f;
    config->bitrate_kbps = 1500;
    config->signaling_port = 8080;
}

int cs_config_load(cs_config *config, const char *path) {
    char line[256];

    if (!config) {
        return -1;
    }

    cs_config_defaults(config);

    if (!path) {
        return 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *eq = strchr(line, '=');
        if (!eq) {
            continue;
        }
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        char *newline = strchr(value, '\n');
        if (newline) {
            *newline = '\0';
        }
        apply_kv(config, key, value);
    }

    fclose(file);
    return 0;
}
