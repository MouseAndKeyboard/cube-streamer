#ifndef CS_RENDER_H
#define CS_RENDER_H

#include <stdint.h>
#include <stddef.h>

typedef struct cs_renderer cs_renderer;

typedef struct {
    int width;
    int height;
    float fps;
} cs_render_config;

cs_renderer *cs_render_create(const cs_render_config *config);
void cs_render_destroy(cs_renderer *renderer);

// Renders one frame into the provided RGBA buffer (size width*height*4).
int cs_render_frame(cs_renderer *renderer, uint8_t *rgba_out, size_t rgba_len);

#endif
