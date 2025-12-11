const myId = Math.random().toString(36).substring(2, 10);
document.getElementById("myId").textContent = myId;

const SERVER = "ws://127.0.0.1:8000/" + myId;
let ws = new WebSocket(SERVER);

let pc = new RTCPeerConnection({
    iceServers: [{ urls: "stun:stun.l.google.com:19302" }]
});

pc.onicecandidate = event => {
    if (event.candidate) {
        ws.send(JSON.stringify({
            id: targetId.value,
            type: "candidate",
            candidate: event.candidate
        }));
    }
};

let localStream = null;

async function initMic() {
    localStream = await navigator.mediaDevices.getUserMedia({ audio: true });
    localStream.getTracks().forEach(track => pc.addTrack(track, localStream));
}

initMic();

ws.onmessage = async (event) => {
    const msg = JSON.parse(event.data);
    const from = msg.id;

    switch (msg.type) {
        case "offer":
            await pc.setRemoteDescription(new RTCSessionDescription(msg.description));
            const answer = await pc.createAnswer();
            await pc.setLocalDescription(answer);
            ws.send(JSON.stringify({
                id: from,
                type: "answer",
                description: answer
            }));
            break;

        case "answer":
            await pc.setRemoteDescription(new RTCSessionDescription(msg.description));
            break;

        case "candidate":
            try {
                await pc.addIceCandidate(new RTCIceCandidate(msg.candidate));
            } catch (e) {
                console.error("Bad candidate", e);
            }
            break;
    }
};

document.getElementById("callBtn").onclick = async () => {
    const peer = targetId.value.trim();
    if (!peer) return;

    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);

    ws.send(JSON.stringify({
        id: peer,
        type: "offer",
        description: offer
    }));
};

document.getElementById("hangupBtn").onclick = () => {
    pc.close();
    pc = new RTCPeerConnection();
};
