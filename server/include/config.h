#ifndef CS_CONFIG_H
#define CS_CONFIG_H

typedef struct {
    int width;
    int height;
    float fps;
    int bitrate_kbps;
    int signaling_port;
} cs_config;

int cs_config_load(cs_config *config, const char *path);
void cs_config_defaults(cs_config *config);

#endif
