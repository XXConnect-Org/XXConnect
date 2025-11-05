function copyId() {
    const idElement = document.getElementById('myId');
    const id = idElement.textContent;
    
    navigator.clipboard.writeText(id).then(() => {
        showCopyNotification('ID скопирован!');
    }).catch(err => {
        copyToClipboardFallback(id);
    });
}

function showCopyNotification(message) {
    const notification = document.createElement('div');
    notification.textContent = message;
    notification.className = 'copy-notification';
    document.body.appendChild(notification);
    
    setTimeout(() => {
        document.body.removeChild(notification);
    }, 2000);
}

function copyToClipboardFallback(text) {
    const textArea = document.createElement('textarea');
    textArea.value = text;
    textArea.style.cssText = 'position: fixed; left: -9999px; opacity: 0;';
    document.body.appendChild(textArea);
    textArea.select();
    
    try {
        document.execCommand('copy');
        showCopyNotification('ID скопирован!');
    } catch (err) {
        showCopyNotification('Ошибка копирования');
    }
    
    document.body.removeChild(textArea);
}

class VideoCall {
    constructor() {
        this.localStream = null;
        this.remoteStream = null;
        this.connection = null;
        this.dataChannel = null;
        
        this.isCallActive = false;
        this.isIncomingCall = false;
        this.isAudioMuted = false;
        this.isVideoOff = false;
        
        this.id = this.generateId();
        this.remoteId = null;
        
        this.initializeElements();
        this.setupEventListeners();
        this.initializeSignaling();
    }

    generateId() {
        return Math.random().toString(36).substring(2, 15) + 
               Math.random().toString(36).substring(2, 15);
    }

    initializeElements() {
        this.localVideo = document.getElementById('localVideo');
        this.remoteVideo = document.getElementById('remoteVideo');
        
        this.acceptButton = document.getElementById('acceptButton');
        this.hangupButton = document.getElementById('hangupButton');
        this.muteButton = document.getElementById('muteButton');
        this.videoButton = document.getElementById('videoButton');
        this.joinButton = document.getElementById('joinButton');
        this.declineButton = document.getElementById('declineButton');
        this.incomingCallButtons = document.getElementById('incomingCallButtons');
        this.statusContainer = document.getElementById('statusContainer');
        
        this.statusMessage = document.getElementById('statusMessage');
        this.connectionStatus = document.getElementById('connectionStatus');
        this.localInfo = document.getElementById('localInfo');
        this.remoteInfo = document.getElementById('remoteInfo');
        
        this.callInput = document.getElementById('callInput');
        this.myIdDisplay = document.getElementById('myId');
        
        if (this.myIdDisplay) {
            this.myIdDisplay.textContent = this.id;
        }
    }

    setupEventListeners() {
        this.acceptButton.addEventListener('click', () => this.acceptCall());
        this.hangupButton.addEventListener('click', () => this.hangUp());
        this.muteButton.addEventListener('click', () => this.toggleMute());
        this.videoButton.addEventListener('click', () => this.toggleVideo());
        this.joinButton.addEventListener('click', () => this.joinCall());
        this.declineButton.addEventListener('click', () => this.declineCall());
    }

    initializeSignaling() {
        console.log(`ID: ${this.id}`);
        
        setTimeout(() => {
            this.simulateIncomingCall();
        }, 3000);
    }

    simulateIncomingCall() {
        const fakeCallerId = 'user_' + Math.random().toString(36).substring(2, 8);
        this.showIncomingCall(fakeCallerId, "Случайный пользователь");
    }

    showIncomingCall(callerId, callerName) {
        this.isIncomingCall = true;
        this.remoteId = callerId;
        
        this.incomingCallButtons.style.display = 'flex';
        this.hangupButton.style.display = 'none';
        
        document.getElementById('callerName').textContent = callerName;
        
        this.showStatus(`Входящий вызов от ${callerName}`, 'incoming');
    }

    async acceptCall() {
        if (!this.isIncomingCall) return;
        
        this.showStatus('Принимаем вызов...', 'connecting');
        const cameraStarted = await this.startCamera();

        if (!cameraStarted) {
            return;
        }

        await this.createConnection();
        
        this.isCallActive = true;
        this.isIncomingCall = false;

        this.simulateRemoteConnection();
        
        this.showActiveCallUI();
        this.showStatus('Соединение установлено!', 'success');
    }

    async joinCall() {
        const remoteId = this.callInput.value.trim();
        if (!remoteId) {
            alert('Введите ID');
            return;
        }

        this.remoteId = remoteId;
        this.showStatus(`Подключаемся к ${remoteId}...`, 'connecting');
        
        const cameraStarted = await this.startCamera();
        if (!cameraStarted) {
            return;
        }

        await this.createConnection(true);
        
        this.isCallActive = true;
        this.showActiveCallUI();
    }

    showActiveCallUI() {
        this.incomingCallButtons.style.display = 'none';
        
        this.hangupButton.style.display = 'block';
        this.hangupButton.disabled = false;
        
        this.joinButton.disabled = true;
        this.callInput.disabled = true;
        
        this.muteButton.disabled = false;
        this.videoButton.disabled = false;
        
        this.statusContainer.style.display = 'block';
    }

    resetUI() {
        this.incomingCallButtons.style.display = 'none';
        this.hangupButton.style.display = 'none';
        
        this.statusContainer.style.display = 'none';
        
        this.joinButton.disabled = false;
        this.callInput.disabled = false;
        
        this.muteButton.disabled = true;
        this.videoButton.disabled = true;
    }

    showStatus(message, type = 'info') {
        this.statusContainer.style.display = 'block';
        this.statusMessage.textContent = message;
        this.statusMessage.className = '';
        
        if (type === 'connecting' || type === 'incoming') {
            this.statusMessage.classList.add('connecting');
        }
        
        console.log(`Статус: ${message}`);
    }

    hideStatus() {
        this.statusContainer.style.display = 'none';
    }

    async createConnection(isCaller = false) {
        try {
            const configuration = {
                iceServers: [
                    { urls: 'stun:stun.l.google.com:19302' },
                    { urls: 'stun:stun1.l.google.com:19302' }
                ]
            };

            this.connection = new RTCConnection(configuration);

            if (this.localStream) {
                this.localStream.getTracks().forEach(track => {
                    this.connection.addTrack(track, this.localStream);
                });
            }

            this.connection.ontrack = (event) => {
                this.remoteStream = event.streams[0];
                this.remoteVideo.srcObject = this.remoteStream;
                this.remoteInfo.textContent = 'Подключено';
            };

            this.connection.onicecandidate = (event) => {
                if (event.candidate) {
                    console.log('ICE candidate:', event.candidate);
                }
            };

            this.connection.onconnectionstatechange = () => {
                this.connectionStatus.textContent = this.connection.connectionState;
                
                switch(this.connection.connectionState) {
                    case 'connected':
                        this.connectionStatus.style.color = '#4CAF50';
                        this.showStatus('Соединение установлено!', 'success');
                        break;
                    case 'disconnected':
                    case 'failed':
                        this.connectionStatus.style.color = '#f44336';
                        this.showStatus('Соединение прервано', 'error');
                        break;
                    case 'connecting':
                        this.connectionStatus.style.color = '#ff9800';
                        this.showStatus('Устанавливаем соединение...', 'connecting');
                        break;
                }
            };

            if (isCaller) {
                this.dataChannel = this.connection.createDataChannel('chat');
                this.setupDataChannel();
            } else {
                this.connection.ondatachannel = (event) => {
                    this.dataChannel = event.channel;
                    this.setupDataChannel();
                };
            }

            if (isCaller) {
                const offer = await this.connection.createOffer();
                await this.connection.setLocalDescription(offer);
                console.log('Offer created:', offer);
                setTimeout(() => this.simulateAnswer(), 1000);
            }

        } catch (error) {
            console.error('Ошибка создания соединения:', error);
            this.showStatus('Ошибка соединения', 'error');
        }
    }

    async simulateAnswer() {
        if (!this.connection) return;
        
        const answer = {
            type: 'answer',
            sdp: 'simulated-answer-sdp'
        };
        
        try {
            await this.connection.setRemoteDescription(answer);
        } catch (error) {
            console.error('Ошибка установки answer:', error);
        }
    }

    setupDataChannel() {
        this.dataChannel.onopen = () => {
            console.log('Data channel opened');
            this.dataChannel.send('Привет от ' + this.id);
        };

        this.dataChannel.onmessage = (event) => {
            console.log('Получено сообщение:', event.data);
        };
    }

    async startCamera() {
        try {
            this.localStream = await navigator.mediaDevices.getUserMedia({
                video: { width: 1280, height: 720 },
                audio: { echoCancellation: true, noiseSuppression: true }
            });

            this.localVideo.srcObject = this.localStream;
            this.localInfo.textContent = 'Камера включена';
            return true;
            
        } catch (error) {
            console.error('Ошибка доступа к камере:', error);
            alert('Ошибка доступа к камере. Проверьте разрешения');
            this.showDemoVideo();
            return false;
        }
    }

    simulateRemoteConnection() {
        setTimeout(() => {
            this.remoteInfo.textContent = 'Подключено';
            this.connectionStatus.textContent = 'Подключено';
            this.connectionStatus.style.color = '#4CAF50';
            
            this.createDemoRemoteVideo();
            
            this.showStatus('Звонок активен!', 'success');
        }, 2000);
    }

    showDemoVideo() {
        this.localInfo.textContent = 'Демо-режим (без камеры)';
    }

    declineCall() {
        this.isIncomingCall = false;
        this.resetUI();
        this.hideStatus();
    }

    hangUp() {
        this.isCallActive = false;
        this.isIncomingCall = false;
        
        if (this.connection) {
            this.connection.close();
            this.connection = null;
        }
        
        if (this.localStream) {
            this.localStream.getTracks().forEach(track => track.stop());
            this.localStream = null;
        }
        
        if (this.remoteVideo.srcObject) {
            this.remoteVideo.srcObject = null;
        }
        
        this.resetUI();
        this.hideStatus();
        
        this.localInfo.textContent = 'Камера выключена';
        this.remoteInfo.textContent = 'Ожидание подключения...';
    }

    toggleMute() {
        if (!this.localStream) return;
        
        const audioTracks = this.localStream.getAudioTracks();
        if (audioTracks.length > 0) {
            this.isAudioMuted = !this.isAudioMuted;
            audioTracks[0].enabled = !this.isAudioMuted;
            
            this.muteButton.textContent = this.isAudioMuted ? '🎤 Выкл звук' : '🎤 Вкл звук';
            this.muteButton.style.background = this.isAudioMuted ? '#f44336' : '#607d8b';
        }
    }

    toggleVideo() {
        if (!this.localStream) return;
        
        const videoTracks = this.localStream.getVideoTracks();
        if (videoTracks.length > 0) {
            this.isVideoOff = !this.isVideoOff;
            videoTracks[0].enabled = !this.isVideoOff;
            
            this.videoButton.textContent = this.isVideoOff ? '📹 Выкл видео' : '📹 Вкл видео';
            this.videoButton.style.background = this.isVideoOff ? '#f44336' : '#607d8b';
            
            this.localInfo.textContent = this.isVideoOff ? 'Видео выключено' : 'Камера включена';
        }
    }

    updateStatus(message, type = 'info') {
        this.showStatus(message, type);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    new VideoCall();
});