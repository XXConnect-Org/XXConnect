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
        showCopyNotification('ИД скопирован!');
    } catch (err) {
        showCopyNotification('Ошибка копирования');
    }
    
    document.body.removeChild(textArea);
}

class AudioCall {
    constructor() {
        this.localStream = null;
        this.isCallActive = false;
        this.isIncomingCall = false;
        this.isAudioMuted = false;
        
        this.id = this.generateId();
        this.simulationTimer = null;
        
        this.initializeElements();
        this.setupEventListeners();
        this.initializeSignaling();
    }

    generateId() {
        return Math.random().toString(36).substring(2, 15) + 
               Math.random().toString(36).substring(2, 15);
    }

    initializeElements() {
        this.localVisualizer = document.getElementById('localAudioVisualizer');
        this.remoteVisualizer = document.getElementById('remoteAudioVisualizer');
        
        this.acceptButton = document.getElementById('acceptButton');
        this.hangupButton = document.getElementById('hangupButton');
        this.muteButton = document.getElementById('muteButton');
        this.joinButton = document.getElementById('joinButton');
        this.declineButton = document.getElementById('declineButton');
        this.incomingCallButtons = document.getElementById('incomingCallButtons');
        this.statusContainer = document.getElementById('statusContainer');
        this.callerInfo = document.getElementById('callerInfo');
        
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
        this.joinButton.addEventListener('click', () => this.joinCall());
        this.declineButton.addEventListener('click', () => this.declineCall());
    }

    initializeSignaling() {
        console.log(`Ваш ID: ${this.id}`);
        
        // setTimeout(() => {
        //     this.simulateIncomingCall();
        // }, 5000);
    }

    simulateIncomingCall() { //ПЕРЕДЕЛАТЬ НА НОРМ ЗВОНКИ
        if (this.isCallActive || this.isIncomingCall) return;
        
        console.log('Симуляция входящего звонка...');
        
        const fakeCallerId = 'user_' + Math.random().toString(36).substring(2, 8);
        const randomCaller = 'Саша';
        
        this.isIncomingCall = true;
        
        if (this.callerInfo) {
            this.callerInfo.style.display = 'block';
        }
        
        document.getElementById('callerName').textContent = randomCaller;
        
        if (this.incomingCallButtons) {
            this.incomingCallButtons.style.display = 'flex';
        }
        
        this.showStatus(`Входящий вызов от ${randomCaller}`, 'incoming');
        
        console.log(`Входящий звонок от: ${randomCaller} (ID: ${fakeCallerId})`);
        
        this.simulationTimer = setTimeout(() => {
            if (this.isIncomingCall) {
                console.log('Звонок автоматически отклонен (таймаут)');
                this.declineCall();
            }
        }, 30000);
    }

    async acceptCall() {
        if (!this.isIncomingCall) return;
        
        console.log('Принимаем звонок...');
        
        if (this.simulationTimer) {
            clearTimeout(this.simulationTimer);
            this.simulationTimer = null;
        }
        
        this.showStatus('Принимаем вызов...', 'connecting');
        
        const micStarted = await this.startMicrophone();
        if (!micStarted) {
            this.showStatus('Не удалось подключить микрофон', 'error');
            return;
        }

        this.isCallActive = true;
        this.isIncomingCall = false;
        
        if (this.callerInfo) {
            this.callerInfo.style.display = 'none';
        }

        this.simulateRemoteConnection();
        this.showActiveCallUI();

        setTimeout(() => {
            if (this.isCallActive) {
                this.showStatus('Подключаемся...', 'connecting');
                
                setTimeout(() => {
                    this.simulateRemoteConnection();
                    this.showActiveCallUI();
                    this.showStatus('Соединение установлено!', 'success');
                }, 2000);
            }
        }, 3000);
    }

    async joinCall() {
        const remoteId = this.callInput.value.trim();
        if (!remoteId) {
            alert('Введите ID собеседника');
            return;
        }

        console.log(`Пытаемся позвонить на ID: ${remoteId}`);
        
        this.showStatus(`Звоним ${remoteId}...`, 'connecting');
        
        const micStarted = await this.startMicrophone();
        if (!micStarted) {
            this.showStatus('Не удалось подключить микрофон', 'error');
            return;
        }

        this.isCallActive = true;
        
        setTimeout(() => {
            if (this.isCallActive) {
                this.showStatus('Собеседник отвечает...', 'connecting');
                
                setTimeout(() => {
                    this.simulateRemoteConnection();
                    this.showActiveCallUI();
                    this.showStatus('Соединение установлено!', 'success');
                }, 2000);
            }
        }, 3000);
    }

    showActiveCallUI() {
        if (this.incomingCallButtons) {
            this.incomingCallButtons.style.display = 'none';
        }
        
        if (this.callerInfo) {
            this.callerInfo.style.display = 'none';
        }
        
        if (this.hangupButton) {
            this.hangupButton.style.display = 'block';
            this.hangupButton.disabled = false;
        }

        if (this.joinButton) {
            this.joinButton.disabled = true;
        }
        if (this.callInput) {
            this.callInput.disabled = true;
        }
        
        if (this.muteButton) {
            this.muteButton.disabled = false;
        }
        
        if (this.statusContainer) {
            this.statusContainer.style.display = 'block';
        }
    }

    simulateRemoteConnection() {
        console.log('Симулируем подключение собеседника...');
        
        if (this.remoteVisualizer) {
            this.remoteVisualizer.classList.add('remote-active');
        }
        
        if (this.remoteInfo) {
            this.remoteInfo.textContent = 'Собеседник подключен';
        }
        
        this.remoteStatusInterval = setInterval(() => {
            if (this.remoteInfo && Math.random() > 0.7) {
                const statuses = ['Говорит...', 'Слушает', 'Подключен'];
                const randomStatus = statuses[Math.floor(Math.random() * statuses.length)];
                this.remoteInfo.textContent = randomStatus;
            }
        }, 3000);
    }

    resetUI() {
        if (this.incomingCallButtons) {
            this.incomingCallButtons.style.display = 'none';
        }
        
        if (this.callerInfo) {
            this.callerInfo.style.display = 'none';
        }
        
        if (this.hangupButton) {
            this.hangupButton.style.display = 'none';
        }
        
        if (this.statusContainer) {
            this.statusContainer.style.display = 'none';
        }
        
        if (this.joinButton) {
            this.joinButton.disabled = false;
        }
        if (this.callInput) {
            this.callInput.disabled = false;
        }
        
        if (this.muteButton) {
            this.muteButton.disabled = true;
            this.muteButton.textContent = 'Вкл/Выкл звук';
            this.muteButton.style.background = '#607d8b';
        }
        
        if (this.localVisualizer) {
            this.localVisualizer.className = 'audio-visualizer';
        }
        if (this.remoteVisualizer) {
            this.remoteVisualizer.className = 'audio-visualizer';
        }
        
        if (this.remoteStatusInterval) {
            clearInterval(this.remoteStatusInterval);
            this.remoteStatusInterval = null;
        }
    }

    showStatus(message, type = 'info') {
        if (!this.statusContainer || !this.statusMessage) return;
        
        this.statusContainer.style.display = 'block';
        this.statusMessage.textContent = message;
        
        this.statusMessage.classList.remove('connecting', 'success', 'error');
        
        if (type === 'connecting' || type === 'incoming') {
            this.statusMessage.classList.add('connecting');
        }
        
        console.log(`Статус: ${message}`);
    }

    hideStatus() {
        if (this.statusContainer) {
            this.statusContainer.style.display = 'none';
        }
    }

    async startMicrophone() {
        try {
            console.log('Запрашиваем доступ к микрофону...');
            
            const constraints = {
                audio: {
                    echoCancellation: true,
                    noiseSuppression: true,
                    autoGainControl: true
                },
                video: false
            };

            this.localStream = await navigator.mediaDevices.getUserMedia(constraints);
            
            console.log('Микрофон успешно подключен');
            
            if (this.localVisualizer) {
                this.localVisualizer.classList.add('mic-active');
            }
            if (this.localInfo) {
                this.localInfo.textContent = 'Микрофон включен';
            }
            
            return true;
            
        } catch (error) {
            console.error('Ошибка доступа к микрофону:', error);
            
            let errorMessage = 'Не удалось получить доступ к микрофону. ';
            
            if (error.name === 'NotAllowedError') {
                errorMessage += 'Пожалуйста, разрешите доступ к микрофону в настройках браузера.';
            } else if (error.name === 'NotFoundError') {
                errorMessage += 'Микрофон не найден.';
            } else {
                errorMessage += `Ошибка: ${error.message}`;
            }
            
            alert(errorMessage);
            
            if (this.localVisualizer) {
                this.localVisualizer.classList.add('mic-active');
            }
            if (this.localInfo) {
                this.localInfo.textContent = 'Демо-режим (без микрофона)'; //ДЕМО ПОДУМАТЬ????
            }
            
            return true;
        }
    }

    declineCall() {
        console.log('Звонок отклонен');
        
        if (this.simulationTimer) {
            clearTimeout(this.simulationTimer);
            this.simulationTimer = null;
        }
        
        this.isIncomingCall = false;
        this.resetUI();
        this.hideStatus();
        
        setTimeout(() => {
            if (!this.isCallActive && !this.isIncomingCall) {
                this.simulateIncomingCall();
            }
        }, 10000);
    }

    hangUp() {
        console.log('Завершаем звонок...');
        
        this.isCallActive = false;
        this.isIncomingCall = false;

        if (this.statusMessage) {
            this.statusMessage.textContent = '';
        }
        
        if (this.localStream) {
            this.localStream.getTracks().forEach(track => {
                track.stop();
            });
            this.localStream = null;
        }
        
        this.resetUI();
        
        if (this.localInfo) {
            this.localInfo.textContent = 'Микрофон выключен';
        }
        if (this.remoteInfo) {
            this.remoteInfo.textContent = 'Ожидание подключения...';
        }
        
        setTimeout(() => {
            if (!this.isCallActive && !this.isIncomingCall) {
                this.simulateIncomingCall();
            }
        }, 15000);
    }

    toggleMute() {
        if (!this.localStream) return;
        
        const audioTracks = this.localStream.getAudioTracks();
        if (audioTracks.length > 0) {
            this.isAudioMuted = !this.isAudioMuted;
            audioTracks[0].enabled = !this.isAudioMuted;
            
            console.log(`Микрофон: ${this.isAudioMuted ? 'отключен' : 'включен'}`);
            
            if (this.muteButton) {
                this.muteButton.textContent = this.isAudioMuted ? 'Включить звук' : 'Выключить звук';
                this.muteButton.style.background = this.isAudioMuted ? '#f44336' : '#607d8b';
            }
            
            if (this.localVisualizer) {
                if (this.isAudioMuted) {
                    this.localVisualizer.classList.remove('mic-active');
                    if (this.localInfo) {
                        this.localInfo.textContent = 'Микрофон отключен';
                    }
                } else {
                    this.localVisualizer.classList.add('mic-active');
                    if (this.localInfo) {
                        this.localInfo.textContent = 'Микрофон включен';
                    }
                }
            }
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
    try {
        new AudioCall();
        console.log('Аудиозвонок инициализирован');
        console.log('Через 5 секунд начнется симуляция входящего звонка...');
    } catch (error) {
        console.error('Ошибка инициализации аудиозвонка:', error);
        alert('Ошибка загрузки приложения. Пожалуйста, обновите страницу.');
    }
});
