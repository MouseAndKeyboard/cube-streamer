# Ops

## Ports

- 8080: signaling WebSocket
- 3478: STUN/TURN (UDP/TCP)
- 49152-65535: WebRTC RTP/RTCP (UDP)

## TURN

Use coturn with a static secret or user/pass. Provide TURN URI to the client.
