#ifndef CS_SIGNALING_H
#define CS_SIGNALING_H

typedef struct cs_signaling cs_signaling;

typedef struct {
    int port;
} cs_signaling_config;

typedef struct {
    void *user;
    void (*on_offer_needed)(void *user);
    void (*on_local_sdp)(void *user, const char *type, const char *sdp);
    void (*on_remote_sdp)(void *user, const char *type, const char *sdp);
    void (*on_remote_ice)(void *user, const char *candidate, int sdp_mline_index, const char *sdp_mid);
} cs_signaling_callbacks;

cs_signaling *cs_signaling_create(const cs_signaling_config *config, const cs_signaling_callbacks *callbacks);
void cs_signaling_destroy(cs_signaling *signaling);

int cs_signaling_broadcast_sdp(cs_signaling *signaling, const char *type, const char *sdp);
int cs_signaling_broadcast_ice(cs_signaling *signaling, const char *candidate, int sdp_mline_index, const char *sdp_mid);

int cs_signaling_poll(cs_signaling *signaling);

#endif
