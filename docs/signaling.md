# Signaling

Signaling uses a single WebSocket per client to exchange:

- `offer` / `answer` (SDP)
- `ice` candidates

Messages are JSON:

```json
{ "type": "offer", "sdp": "..." }
{ "type": "answer", "sdp": "..." }
{ "type": "ice", "candidate": "...", "sdpMLineIndex": 0, "sdpMid": "0" }
```

The server is the offerer by default. Each WebSocket connection maps to one WebRTC peer session.
