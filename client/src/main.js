const connectButton = document.getElementById('connect');
const statusEl = document.getElementById('status');
const wsUrlInput = document.getElementById('ws-url');
const videoEl = document.getElementById('video');

let ws;
let pc;

function setStatus(text) {
  statusEl.textContent = text;
}

async function createPeerConnection() {
  pc = new RTCPeerConnection({
    iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
  });

  pc.ontrack = (event) => {
    if (videoEl.srcObject !== event.streams[0]) {
      videoEl.srcObject = event.streams[0];
    }
  };

  pc.onicecandidate = (event) => {
    if (event.candidate && ws?.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({
        type: 'ice',
        candidate: event.candidate.candidate,
        sdpMLineIndex: event.candidate.sdpMLineIndex,
        sdpMid: event.candidate.sdpMid
      }));
    }
  };
}

connectButton.addEventListener('click', async () => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    return;
  }

  setStatus('connecting');
  ws = new WebSocket(wsUrlInput.value);

  ws.onopen = async () => {
    await createPeerConnection();
    setStatus('connected');
  };

  ws.onmessage = async (event) => {
    const message = JSON.parse(event.data);
    if (message.type === 'offer') {
      await pc.setRemoteDescription({ type: 'offer', sdp: message.sdp });
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);
      ws.send(JSON.stringify({ type: 'answer', sdp: answer.sdp }));
    } else if (message.type === 'ice') {
      if (message.candidate) {
        await pc.addIceCandidate({
          candidate: message.candidate,
          sdpMid: message.sdpMid,
          sdpMLineIndex: message.sdpMLineIndex
        });
      }
    }
  };

  ws.onclose = () => {
    setStatus('closed');
  };

  ws.onerror = () => {
    setStatus('error');
  };
});
