let ws = null;
let pc = null;
let localStream = null;

const remoteAudio = document.getElementById("remote");
document.getElementById("connect").onclick = connect;
document.getElementById("call").onclick = startCall;
document.getElementById("hangup").onclick = hangUp;

function genId() {
    return Math.random().toString(36).substring(2, 12);
}

async function connect() {
    ws = new WebSocket("ws://127.0.0.1:8000/" + genId());

    ws.onopen = () => {
        console.log("Connected to signaling");
        document.getElementById("call").disabled = false;
    };

    ws.onmessage = async (ev) => {
        const msg = JSON.parse(ev.data);

        if (!pc) await createPeerConnection();

        if (msg.type === "offer") {
            await pc.setRemoteDescription({ type: "offer", sdp: msg.description });
            const answer = await pc.createAnswer();
            await pc.setLocalDescription(answer);

            ws.send(JSON.stringify({
                type: "answer",
                description: answer.sdp,
                id: msg.id
            }));
        }

        if (msg.type === "answer") {
            await pc.setRemoteDescription({ type: "answer", sdp: msg.description });
        }

        if (msg.type === "candidate") {
            await pc.addIceCandidate({
                candidate: msg.candidate,
                sdpMid: msg.mid
            });
        }
    };
}

async function createPeerConnection() {
    pc = new RTCPeerConnection();

    pc.onicecandidate = (e) => {
        if (e.candidate) {
            ws.send(JSON.stringify({
                type: "candidate",
                candidate: e.candidate.candidate,
                mid: e.candidate.sdpMid
            }));
        }
    };

    pc.ontrack = (event) => {
        remoteAudio.srcObject = event.streams[0];
    };

    if (!localStream) {
        localStream = await navigator.mediaDevices.getUserMedia({ audio: true });
    }

    localStream.getTracks().forEach(t => pc.addTrack(t, localStream));
}

async function startCall() {
    if (!pc) await createPeerConnection();

    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);

    ws.send(JSON.stringify({
        type: "offer",
        description: offer.sdp
    }));

    document.getElementById("hangup").disabled = false;
}

function hangUp() {
    if (pc) pc.close();
    pc = null;

    document.getElementById("hangup").disabled = true;
}
