#include "signaling.h"

#include <libwebsockets.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct cs_signaling {
    struct lws_context *context;
    cs_signaling_callbacks callbacks;
    int port;
    struct lws *client_wsi;
    char *pending;
};

static char *json_escape(const char *input) {
    size_t len = strlen(input);
    size_t extra = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = input[i];
        if (c == '"' || c == '\\') {
            extra += 1;
        } else if (c == '\n' || c == '\r') {
            extra += 1;
        }
    }
    char *out = (char *)malloc(len + extra + 1);
    if (!out) {
        return NULL;
    }
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = input[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = c;
        } else if (c == '\n') {
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (c == '\r') {
            out[j++] = '\\';
            out[j++] = 'r';
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}

static void json_unescape_inplace(char *value) {
    size_t len = strlen(value);
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        if (value[i] == '\\' && i + 1 < len) {
            char next = value[i + 1];
            if (next == 'n') {
                value[j++] = '\n';
                i++;
                continue;
            }
            if (next == 'r') {
                value[j++] = '\r';
                i++;
                continue;
            }
            if (next == '"' || next == '\\') {
                value[j++] = next;
                i++;
                continue;
            }
        }
        value[j++] = value[i];
    }
    value[j] = '\0';
}

static int extract_json_string(const char *json, const char *key, char *out, size_t out_len) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (!pos) {
        return -1;
    }
    pos = strchr(pos, ':');
    if (!pos) {
        return -1;
    }
    pos = strchr(pos, '"');
    if (!pos) {
        return -1;
    }
    pos++;
    const char *end = strchr(pos, '"');
    if (!end) {
        return -1;
    }
    size_t len = (size_t)(end - pos);
    if (len >= out_len) {
        len = out_len - 1;
    }
    memcpy(out, pos, len);
    out[len] = '\0';
    json_unescape_inplace(out);
    return 0;
}

static int extract_json_int(const char *json, const char *key, int *out_value) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (!pos) {
        return -1;
    }
    pos = strchr(pos, ':');
    if (!pos) {
        return -1;
    }
    pos++;
    *out_value = atoi(pos);
    return 0;
}

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    (void)user;
    (void)len;
    cs_signaling *signaling = (cs_signaling *)lws_context_user(lws_get_context(wsi));

    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        signaling->client_wsi = wsi;
        if (signaling->callbacks.on_offer_needed) {
            signaling->callbacks.on_offer_needed(signaling->callbacks.user);
        }
        break;
    case LWS_CALLBACK_RECEIVE: {
        const char *payload = (const char *)in;
        char type[16];
        if (extract_json_string(payload, "type", type, sizeof(type)) != 0) {
            break;
        }

        if (strcmp(type, "answer") == 0 || strcmp(type, "offer") == 0) {
            char sdp[8192];
            if (extract_json_string(payload, "sdp", sdp, sizeof(sdp)) == 0) {
                signaling->callbacks.on_remote_sdp(signaling->callbacks.user, type, sdp);
            }
        } else if (strcmp(type, "ice") == 0) {
            char candidate[1024];
            char sdp_mid[32] = "0";
            int sdp_mline_index = 0;
            if (extract_json_string(payload, "candidate", candidate, sizeof(candidate)) == 0) {
                extract_json_string(payload, "sdpMid", sdp_mid, sizeof(sdp_mid));
                extract_json_int(payload, "sdpMLineIndex", &sdp_mline_index);
                signaling->callbacks.on_remote_ice(signaling->callbacks.user, candidate, sdp_mline_index, sdp_mid);
            }
        }
        break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        if (!signaling->pending || signaling->client_wsi != wsi) {
            break;
        }
        size_t msg_len = strlen(signaling->pending);
        unsigned char *buf = (unsigned char *)malloc(LWS_PRE + msg_len + 1);
        if (!buf) {
            break;
        }
        memcpy(buf + LWS_PRE, signaling->pending, msg_len);
        lws_write(wsi, buf + LWS_PRE, msg_len, LWS_WRITE_TEXT);
        free(buf);
        free(signaling->pending);
        signaling->pending = NULL;
        break;
    }
    case LWS_CALLBACK_CLOSED:
        if (signaling->client_wsi == wsi) {
            signaling->client_wsi = NULL;
        }
        break;
    default:
        break;
    }

    return 0;
}

cs_signaling *cs_signaling_create(const cs_signaling_config *config, const cs_signaling_callbacks *callbacks) {
    if (!config || !callbacks) {
        return NULL;
    }

    cs_signaling *signaling = (cs_signaling *)calloc(1, sizeof(cs_signaling));
    if (!signaling) {
        return NULL;
    }

    signaling->callbacks = *callbacks;
    signaling->port = config->port;

    static struct lws_protocols protocols[] = {
        { "cs-signaling", ws_callback, 0, 8192 },
        { NULL, NULL, 0, 0 }
    };

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = signaling->port;
    info.protocols = protocols;
    info.user = signaling;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    signaling->context = lws_create_context(&info);
    if (!signaling->context) {
        free(signaling);
        return NULL;
    }

    return signaling;
}

void cs_signaling_destroy(cs_signaling *signaling) {
    if (!signaling) {
        return;
    }

    if (signaling->context) {
        lws_context_destroy(signaling->context);
    }

    free(signaling->pending);
    free(signaling);
}

static int send_pending(cs_signaling *signaling) {
    if (!signaling || !signaling->client_wsi) {
        return -1;
    }
    lws_callback_on_writable(signaling->client_wsi);
    return 0;
}

int cs_signaling_broadcast_sdp(cs_signaling *signaling, const char *type, const char *sdp) {
    if (!signaling || !type || !sdp) {
        return -1;
    }

    free(signaling->pending);
    char *escaped = json_escape(sdp);
    if (!escaped) {
        return -1;
    }
    size_t needed = strlen(type) + strlen(escaped) + 32;
    signaling->pending = (char *)malloc(needed);
    if (!signaling->pending) {
        free(escaped);
        return -1;
    }

    snprintf(signaling->pending, needed, "{\"type\":\"%s\",\"sdp\":\"%s\"}", type, escaped);
    free(escaped);
    return send_pending(signaling);
}

int cs_signaling_broadcast_ice(cs_signaling *signaling, const char *candidate, int sdp_mline_index, const char *sdp_mid) {
    if (!signaling || !candidate) {
        return -1;
    }

    free(signaling->pending);
    char *escaped = json_escape(candidate);
    if (!escaped) {
        return -1;
    }
    size_t needed = strlen(escaped) + (sdp_mid ? strlen(sdp_mid) : 1) + 64;
    signaling->pending = (char *)malloc(needed);
    if (!signaling->pending) {
        free(escaped);
        return -1;
    }

    snprintf(signaling->pending, needed,
             "{\"type\":\"ice\",\"candidate\":\"%s\",\"sdpMLineIndex\":%d,\"sdpMid\":\"%s\"}",
             escaped, sdp_mline_index, sdp_mid ? sdp_mid : "0");
    free(escaped);
    return send_pending(signaling);
}

int cs_signaling_poll(cs_signaling *signaling) {
    if (!signaling || !signaling->context) {
        return -1;
    }
    lws_service(signaling->context, 0);
    return 0;
}
