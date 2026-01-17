#include "pipeline.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>
#include <stdlib.h>
#include <string.h>

struct cs_pipeline {
    GstElement *pipeline;
    GstElement *appsrc;
    GstElement *webrtcbin;
    cs_pipeline_config cfg;
};

static GstWebRTCSDPType sdp_type_from_string(const char *type) {
    if (!type) {
        return GST_WEBRTC_SDP_TYPE_OFFER;
    }
    if (strcmp(type, "offer") == 0) {
        return GST_WEBRTC_SDP_TYPE_OFFER;
    }
    if (strcmp(type, "answer") == 0) {
        return GST_WEBRTC_SDP_TYPE_ANSWER;
    }
    if (strcmp(type, "pranswer") == 0) {
        return GST_WEBRTC_SDP_TYPE_PRANSWER;
    }
    return GST_WEBRTC_SDP_TYPE_ROLLBACK;
}

static void on_ice_candidate(GstElement *webrtcbin, guint mlineindex, gchar *candidate, gpointer user_data) {
    cs_pipeline *pipeline = (cs_pipeline *)user_data;
    if (pipeline->cfg.on_local_ice) {
        pipeline->cfg.on_local_ice(pipeline->cfg.user, candidate, (int)mlineindex, "0");
    }
}

cs_pipeline *cs_pipeline_create(const cs_pipeline_config *config) {
    if (!config) {
        return NULL;
    }

    gst_init(NULL, NULL);

    cs_pipeline *pipeline = (cs_pipeline *)calloc(1, sizeof(cs_pipeline));
    if (!pipeline) {
        return NULL;
    }

    pipeline->cfg = *config;

    pipeline->pipeline = gst_pipeline_new("cs-pipeline");
    pipeline->appsrc = gst_element_factory_make("appsrc", "cs-appsrc");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "cs-videoconvert");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "cs-capsfilter");
    GstElement *encoder = gst_element_factory_make("x264enc", "cs-x264enc");
    GstElement *pay = gst_element_factory_make("rtph264pay", "cs-pay");
    pipeline->webrtcbin = gst_element_factory_make("webrtcbin", "cs-webrtcbin");

    if (!pipeline->pipeline || !pipeline->appsrc || !videoconvert || !capsfilter || !encoder || !pay || !pipeline->webrtcbin) {
        cs_pipeline_destroy(pipeline);
        return NULL;
    }

    GstCaps *app_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGBA",
        "width", G_TYPE_INT, pipeline->cfg.width,
        "height", G_TYPE_INT, pipeline->cfg.height,
        "framerate", GST_TYPE_FRACTION, (int)(pipeline->cfg.fps + 0.5f), 1,
        NULL);

    g_object_set(G_OBJECT(pipeline->appsrc),
                 "caps", app_caps,
                 "is-live", TRUE,
                 "format", GST_FORMAT_TIME,
                 "do-timestamp", TRUE,
                 NULL);
    gst_caps_unref(app_caps);

    GstCaps *i420_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "I420",
        NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", i420_caps, NULL);
    gst_caps_unref(i420_caps);

    g_object_set(G_OBJECT(encoder),
                 "tune", 0x00000004, /* zerolatency */
                 "speed-preset", 1, /* ultrafast */
                 "bitrate", pipeline->cfg.bitrate_kbps,
                 NULL);

    g_object_set(G_OBJECT(pay), "config-interval", 1, "pt", 96, NULL);

    const char *stun = getenv("CS_STUN_SERVER");
    if (stun) {
        g_object_set(G_OBJECT(pipeline->webrtcbin), "stun-server", stun, NULL);
    }

    gst_bin_add_many(GST_BIN(pipeline->pipeline),
                     pipeline->appsrc, videoconvert, capsfilter, encoder, pay, pipeline->webrtcbin, NULL);

    if (!gst_element_link_many(pipeline->appsrc, videoconvert, capsfilter, encoder, pay, NULL)) {
        cs_pipeline_destroy(pipeline);
        return NULL;
    }

    GstPad *pay_src = gst_element_get_static_pad(pay, "src");
    GstPad *webrtc_sink = gst_element_get_request_pad(pipeline->webrtcbin, "sink_%u");
    if (!pay_src || !webrtc_sink || gst_pad_link(pay_src, webrtc_sink) != GST_PAD_LINK_OK) {
        if (pay_src) {
            gst_object_unref(pay_src);
        }
        if (webrtc_sink) {
            gst_object_unref(webrtc_sink);
        }
        cs_pipeline_destroy(pipeline);
        return NULL;
    }
    gst_object_unref(pay_src);
    gst_object_unref(webrtc_sink);

    g_signal_connect(pipeline->webrtcbin, "on-ice-candidate", G_CALLBACK(on_ice_candidate), pipeline);

    gst_element_set_state(pipeline->pipeline, GST_STATE_PLAYING);
    return pipeline;
}

void cs_pipeline_destroy(cs_pipeline *pipeline) {
    if (!pipeline) {
        return;
    }

    if (pipeline->pipeline) {
        gst_element_set_state(pipeline->pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline->pipeline);
    }

    free(pipeline);
}

int cs_pipeline_push_frame(cs_pipeline *pipeline, const uint8_t *rgba, size_t len, uint64_t pts_ns) {
    if (!pipeline || !rgba) {
        return -1;
    }

    GstBuffer *buffer = gst_buffer_new_allocate(NULL, len, NULL);
    if (!buffer) {
        return -1;
    }

    gst_buffer_fill(buffer, 0, rgba, len);
    GST_BUFFER_PTS(buffer) = pts_ns;
    GST_BUFFER_DTS(buffer) = pts_ns;
    GST_BUFFER_DURATION(buffer) = (GstClockTime)(GST_SECOND / pipeline->cfg.fps);

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(pipeline->appsrc), buffer);
    return ret == GST_FLOW_OK ? 0 : -1;
}

int cs_pipeline_set_remote_description(cs_pipeline *pipeline, const char *sdp_type, const char *sdp) {
    if (!pipeline || !sdp) {
        return -1;
    }

    GstSDPMessage *sdp_msg = NULL;
    gst_sdp_message_new(&sdp_msg);
    if (gst_sdp_message_parse_buffer((const guint8 *)sdp, strlen(sdp), sdp_msg) != GST_SDP_OK) {
        gst_sdp_message_free(sdp_msg);
        return -1;
    }

    GstWebRTCSessionDescription *desc = gst_webrtc_session_description_new(sdp_type_from_string(sdp_type), sdp_msg);
    g_signal_emit_by_name(pipeline->webrtcbin, "set-remote-description", desc, NULL);
    gst_webrtc_session_description_free(desc);
    return 0;
}

char *cs_pipeline_create_offer(cs_pipeline *pipeline) {
    if (!pipeline) {
        return NULL;
    }

    GstPromise *promise = gst_promise_new();
    g_signal_emit_by_name(pipeline->webrtcbin, "create-offer", NULL, promise);
    gst_promise_wait(promise);

    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *offer = NULL;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);

    if (!offer) {
        return NULL;
    }

    g_signal_emit_by_name(pipeline->webrtcbin, "set-local-description", offer, NULL);
    char *sdp_text = gst_sdp_message_as_text(offer->sdp);
    gst_webrtc_session_description_free(offer);

    if (!sdp_text) {
        return NULL;
    }

    char *out = strdup(sdp_text);
    g_free(sdp_text);

    if (pipeline->cfg.on_local_sdp && out) {
        pipeline->cfg.on_local_sdp(pipeline->cfg.user, "offer", out);
    }

    return out;
}

int cs_pipeline_add_ice_candidate(cs_pipeline *pipeline, const char *candidate, int sdp_mline_index, const char *sdp_mid) {
    (void)sdp_mid;
    if (!pipeline || !candidate) {
        return -1;
    }

    g_signal_emit_by_name(pipeline->webrtcbin, "add-ice-candidate", sdp_mline_index, candidate);
    return 0;
}
