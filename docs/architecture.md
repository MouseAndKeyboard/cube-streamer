# Architecture

## Overview

The server renders a rotating cube into an offscreen OpenGL framebuffer, encodes frames as H.264, and sends them over WebRTC. The client only decodes and displays the video stream.

## Server Pipeline

1. Render frame (RGBA) via EGL + OpenGL ES.
2. Push frame into GStreamer `appsrc`.
3. Convert/scale and encode H.264.
4. Hand frames to `webrtcbin` for RTP + DTLS + SRTP.
5. Exchange SDP/ICE over WebSocket signaling.

## Client Pipeline

1. Establish WebRTC PeerConnection.
2. Receive video track.
3. Attach to `<video>` element.

## Dependencies

- EGL / OpenGL ES
- GStreamer (app + webrtc plugins)
- libwebsockets
- STUN/TURN (coturn)
