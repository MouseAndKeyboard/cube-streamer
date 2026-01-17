# cube-streamer

A minimal client–server system that streams a slowly rotating 3D cube from a backend server to web clients in real time.

## Goal

The purpose of this project is to build a small, end-to-end streaming pipeline:

- A **server** renders a simple 3D scene containing a single cube.
- The cube rotates continuously at a slow, constant rate.
- The rendered output is **streamed live** to one or more **web clients**.
- Clients view the stream in a browser with minimal setup.

This is intentionally simple: one object, one animation, one stream. The focus is on understanding the mechanics of real-time rendering, encoding, transport, and playback rather than graphics complexity.

## Non-Goals

- No complex lighting, textures, or assets.
- No gameplay or user interaction (initially).
- No heavy engine abstractions unless required.
- No account systems or persistence.

## Intended Architecture (High Level)

- **Server**
  - Owns the authoritative render loop.
  - Renders a rotating 3D cube (software or GPU-accelerated).
  - Encodes frames for real-time delivery.
  - Streams frames over the network.

- **Client**
  - Runs in a standard web browser.
  - Connects to the server via a streaming protocol.
  - Decodes and displays the video stream with low latency.

## Why This Exists

This repo is a learning and experimentation vehicle for:

- Real-time streaming architectures
- Client/server separation for rendered content
- Latency, bitrate, and transport trade-offs
- Foundations for more complex streamed 3D or cloud-rendered systems

If this works well, the cube can later be replaced with more complex scenes, inputs, or bidirectional control.

## Status

Early scaffold. Only the project intent is defined so far.

## Next Steps

- Choose a rendering approach for the server.
- Choose a transport and streaming protocol.
- Implement the simplest possible rotating cube.
- Get first pixels on a web client.

---

Minimal by design.
