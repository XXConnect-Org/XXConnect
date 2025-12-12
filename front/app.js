let ws = null;
let backendConnected = false;
let audioContext = null;

const connectBtn = document.getElementById("connectBtn");
const startAudioBtn = document.getElementById("startAudioBtn");
const stopAudioBtn = document.getElementById("stopAudioBtn");
const roomInput = document.getElementById("room");
const statusText = document.getElementById("statusText");
const volumeSlider = document.getElementById("volumeSlider");

connectBtn.onclick = () => {
    const room = roomInput.value.trim();
    if (!room) return;

    ws = new WebSocket(`ws://127.0.0.1:9002/room/${room}`);

    ws.onopen = () => {
        backendConnected = true;
        statusText.textContent = "Connected to backend";
    };

    ws.onmessage = (msg) => {
        const data = JSON.parse(msg.data);

        if (data.type === "status") {
            statusText.textContent = data.message;
        }

        if (data.type === "audio") {
            // PCM audio chunk from backend
            playAudio(data.samples);
        }
    };
};

startAudioBtn.onclick = () => {
    if (!backendConnected) return;
    ws.send(JSON.stringify({ action: "startAudio" }));
};

stopAudioBtn.onclick = () => {
    if (!backendConnected) return;
    ws.send(JSON.stringify({ action: "stopAudio" }));
};

volumeSlider.oninput = () => {
    if (!backendConnected) return;
    ws.send(JSON.stringify({
        action: "setVolume",
        value: Number(volumeSlider.value)
    }));
};

function playAudio(samples) {
    if (!audioContext) {
        audioContext = new AudioContext({ sampleRate: 48000 });
    }

    const buffer = audioContext.createBuffer(1, samples.length, 48000);
    buffer.copyToChannel(new Float32Array(samples), 0);

    const source = audioContext.createBufferSource();
    source.buffer = buffer;
    source.connect(audioContext.destination);
    source.start();
}
