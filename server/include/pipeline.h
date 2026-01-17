#ifndef CS_PIPELINE_H
#define CS_PIPELINE_H

#include <stdint.h>
#include <stddef.h>

typedef struct cs_pipeline cs_pipeline;

typedef struct {
    int width;
    int height;
    float fps;
    int bitrate_kbps;
    void *user;
    void (*on_local_sdp)(void *user, const char *type, const char *sdp);
    void (*on_local_ice)(void *user, const char *candidate, int sdp_mline_index, const char *sdp_mid);
} cs_pipeline_config;

cs_pipeline *cs_pipeline_create(const cs_pipeline_config *config);
void cs_pipeline_destroy(cs_pipeline *pipeline);

// Push a raw RGBA frame into the pipeline.
int cs_pipeline_push_frame(cs_pipeline *pipeline, const uint8_t *rgba, size_t len, uint64_t pts_ns);

// Signaling hooks for SDP/ICE exchange.
int cs_pipeline_set_remote_description(cs_pipeline *pipeline, const char *sdp_type, const char *sdp);
char *cs_pipeline_create_offer(cs_pipeline *pipeline);
int cs_pipeline_add_ice_candidate(cs_pipeline *pipeline, const char *candidate, int sdp_mline_index, const char *sdp_mid);

#endif
