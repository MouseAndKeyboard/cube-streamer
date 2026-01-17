#include "config.h"
#include "pipeline.h"
#include "render.h"
#include "signaling.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint64_t monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

typedef struct {
    cs_pipeline *pipeline;
    cs_signaling *signaling;
} cs_app;

static void on_offer_needed(void *user) {
    cs_app *app = (cs_app *)user;
    char *offer = cs_pipeline_create_offer(app->pipeline);
    if (offer) {
        free(offer);
    }
}

static void on_local_sdp(void *user, const char *type, const char *sdp) {
    cs_app *app = (cs_app *)user;
    cs_signaling_broadcast_sdp(app->signaling, type, sdp);
}

static void on_remote_sdp(void *user, const char *type, const char *sdp) {
    cs_app *app = (cs_app *)user;
    cs_pipeline_set_remote_description(app->pipeline, type, sdp);
}

static void on_remote_ice(void *user, const char *candidate, int sdp_mline_index, const char *sdp_mid) {
    cs_app *app = (cs_app *)user;
    cs_pipeline_add_ice_candidate(app->pipeline, candidate, sdp_mline_index, sdp_mid);
}

static void on_local_ice(void *user, const char *candidate, int sdp_mline_index, const char *sdp_mid) {
    cs_app *app = (cs_app *)user;
    cs_signaling_broadcast_ice(app->signaling, candidate, sdp_mline_index, sdp_mid);
}

int main(int argc, char **argv) {
    const char *config_path = NULL;
    if (argc > 1) {
        config_path = argv[1];
    }

    cs_config config;
    if (cs_config_load(&config, config_path) != 0) {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }

    cs_render_config render_cfg = {
        .width = config.width,
        .height = config.height,
        .fps = config.fps
    };
    cs_renderer *renderer = cs_render_create(&render_cfg);
    if (!renderer) {
        fprintf(stderr, "Renderer init failed\n");
        return 1;
    }

    cs_app app = { .pipeline = NULL, .signaling = NULL };

    cs_pipeline_config pipeline_cfg = {
        .width = config.width,
        .height = config.height,
        .fps = config.fps,
        .bitrate_kbps = config.bitrate_kbps,
        .user = &app,
        .on_local_sdp = on_local_sdp,
        .on_local_ice = on_local_ice
    };
    cs_pipeline *pipeline = cs_pipeline_create(&pipeline_cfg);
    if (!pipeline) {
        fprintf(stderr, "Pipeline init failed\n");
        cs_render_destroy(renderer);
        return 1;
    }

    cs_signaling_config signaling_cfg = { .port = config.signaling_port };
    cs_signaling_callbacks callbacks = {
        .user = &app,
        .on_offer_needed = on_offer_needed,
        .on_local_sdp = on_local_sdp,
        .on_remote_sdp = on_remote_sdp,
        .on_remote_ice = on_remote_ice
    };
    cs_signaling *signaling = cs_signaling_create(&signaling_cfg, &callbacks);
    if (!signaling) {
        fprintf(stderr, "Signaling init failed\n");
        cs_pipeline_destroy(pipeline);
        cs_render_destroy(renderer);
        return 1;
    }

    app.pipeline = pipeline;
    app.signaling = signaling;

    const size_t frame_size = (size_t)config.width * (size_t)config.height * 4;
    uint8_t *frame = (uint8_t *)malloc(frame_size);
    if (!frame) {
        fprintf(stderr, "Frame buffer alloc failed\n");
        cs_signaling_destroy(signaling);
        cs_pipeline_destroy(pipeline);
        cs_render_destroy(renderer);
        return 1;
    }

    const uint64_t frame_ns = (uint64_t)(1000000000.0 / config.fps);
    uint64_t next_tick = monotonic_ns();

    while (1) {
        uint64_t now = monotonic_ns();
        if (now < next_tick) {
            struct timespec sleep_ts = { .tv_sec = 0, .tv_nsec = (long)(next_tick - now) };
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        if (cs_render_frame(renderer, frame, frame_size) == 0) {
            cs_pipeline_push_frame(pipeline, frame, frame_size, now);
        }

        cs_signaling_poll(signaling);
        next_tick += frame_ns;
    }

    free(frame);
    cs_signaling_destroy(signaling);
    cs_pipeline_destroy(pipeline);
    cs_render_destroy(renderer);
    return 0;
}
