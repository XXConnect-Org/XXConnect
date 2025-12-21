// Копирование URL комнаты в буфер обмена
function copyRoomUrl() {
    const urlElement = document.getElementById('roomUrl');
    const url = urlElement.textContent;

    navigator.clipboard.writeText(url).then(() => {
        showCopyNotification('Ссылка скопирована!');
    }).catch(err => {
        copyToClipboardFallback(url);
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
        showCopyNotification('Ссылка скопирована!');
    } catch (err) {
        showCopyNotification('Ошибка копирования');
    }

    document.body.removeChild(textArea);
}

// Основной класс для видеозвонков с WebRTC
class VideoCall {
    constructor() {
        this.localStream = null;
        this.remoteStream = null;
        this.isCallActive = false;
        this.isAudioMuted = false;
        this.isVideoMuted = false;
        this.room = null;
        this.pc = null;
        this.isInitiator = false;
        this.membersCount = 0;
        this.pendingCandidates = []; // Буфер для ожидающих ICE кандидатов

        // DOM элементы
        this.localVideo = document.getElementById('localVideo');
        this.remoteVideo = document.getElementById('remoteVideo');
        this.localVideoContainer = document.getElementById('localVideoContainer');
        this.remoteVideoContainer = document.getElementById('remoteVideoContainer');
        this.localVideoOverlay = document.getElementById('localVideoOverlay');
        this.remoteVideoOverlay = document.getElementById('remoteVideoOverlay');
        this.roomUrlDisplay = document.getElementById('roomUrl');

        this.acceptButton = document.getElementById('acceptButton');
        this.hangupButton = document.getElementById('hangupButton');
        this.muteButton = document.getElementById('muteButton');
        this.videoButton = document.getElementById('videoButton');
        this.declineButton = document.getElementById('declineButton');
        this.incomingCallButtons = document.getElementById('incomingCallButtons');
        this.statusContainer = document.getElementById('statusContainer');
        this.callerInfo = document.getElementById('callerInfo');

        this.statusMessage = document.getElementById('statusMessage');
        this.connectionStatus = document.getElementById('connectionStatus');
        this.localInfo = document.getElementById('localInfo');
        this.remoteInfo = document.getElementById('remoteInfo');

        // ScaleDrone настройки
        this.channelId = 'hDgkS2GBMa2Klonx'; // Замените на ваш Channel ID
        this.drone = null;

        // Генерация или получение хэша комнаты из URL
        this.roomHash = this.getOrCreateRoomHash();
        this.roomName = 'observable-' + this.roomHash;

        this.initializeElements();
        this.setupEventListeners();
        this.initializeWebRTC();
        this.updateRoomUrl();
    }

    getOrCreateRoomHash() {
        if (!location.hash) {
            location.hash = Math.floor(Math.random() * 0xFFFFFF).toString(16);
        }
        return location.hash.substring(1);
    }

    updateRoomUrl() {
        const fullUrl = window.location.href;
        if (this.roomUrlDisplay) {
            this.roomUrlDisplay.textContent = fullUrl;
        }
    }

    initializeElements() {
        // Инициализация уже выполнена в конструкторе
    }

    setupEventListeners() {
        this.acceptButton.addEventListener('click', () => this.acceptCall());
        this.hangupButton.addEventListener('click', () => this.hangUp());
        this.muteButton.addEventListener('click', () => this.toggleMute());
        this.videoButton.addEventListener('click', () => this.toggleVideo());
        this.declineButton.addEventListener('click', () => this.declineCall());
    }

    initializeWebRTC() {
        // Подключение к ScaleDrone
        this.drone = new ScaleDrone(this.channelId);

        this.drone.on('open', error => {
            if (error) {
                console.error('Ошибка подключения к ScaleDrone:', error);
                this.showStatus('Ошибка подключения к серверу', 'error');
                return;
            }

            console.log('Подключено к ScaleDrone как клиент:', this.drone.clientId);

            // Подписка на комнату
            this.room = this.drone.subscribe(this.roomName);

            this.room.on('open', error => {
                if (error) {
                    console.error('Ошибка подписки на комнату:', error);
                    this.showStatus('Ошибка подключения к комнате', 'error');
                    return;
                }
                console.log('Подписаны на комнату:', this.roomName);
            });

            // Обработка списка участников
            this.room.on('members', members => {
                console.log('Участники в комнате:', members.map(m => m.id));
                this.membersCount = members.length;

                if (members.length >= 2) {
                    // Сортируем для стабильного определения ролей
                    const sortedMembers = [...members].sort((a, b) => a.id.localeCompare(b.id));
                    const myIndex = sortedMembers.findIndex(member => member.id === this.drone.clientId);
                    this.isInitiator = (myIndex === 1); // Второй в отсортированном списке

                    console.log('Отсортированные участники:', sortedMembers.map(m => m.id));
                    console.log('Мой индекс в отсортированном списке:', myIndex);
                    console.log('Я инициатор:', this.isInitiator);

                    this.startCall();
                } else if (members.length === 1) {
                    this.showStatus('Ожидание подключения собеседника...', 'connecting');
                }
            });

            // Обработка входящих сообщений
            this.room.on('data', (message, client) => {
                // Игнорируем свои сообщения
                if (client.id === this.drone.clientId) {
                    return;
                }

                console.log('Получено сообщение:', message);
                this.handleSignalingMessage(message, client);
            });
        });
    }

    async startCameraAndMicrophone() {
        try {
            console.log('Запрашиваем доступ к камере и микрофону...');

            const constraints = {
                audio: {
                    echoCancellation: true,
                    noiseSuppression: true,
                    autoGainControl: true
                },
                video: {
                    width: { ideal: 640 },
                    height: { ideal: 480 },
                    facingMode: 'user'
                }
            };

            this.localStream = await navigator.mediaDevices.getUserMedia(constraints);

            console.log('Камера и микрофон успешно подключены');

            // Отображаем локальное видео
            this.localVideo.srcObject = this.localStream;
            this.localVideoContainer.classList.add('active');
            this.localVideo.play().catch(e => console.log('Ошибка воспроизведения локального видео:', e));

            if (this.localInfo) {
                this.localInfo.textContent = 'Камера и микрофон включены';
            }

            return true;

        } catch (error) {
            console.error('Ошибка доступа к камере и микрофону:', error);

            let errorMessage = 'Не удалось получить доступ к камере и микрофону. ';

            if (error.name === 'NotAllowedError') {
                errorMessage += 'Пожалуйста, разрешите доступ к камере и микрофону в настройках браузера.';
            } else if (error.name === 'NotFoundError') {
                errorMessage += 'Камера или микрофон не найдены.';
            } else {
                errorMessage += `Ошибка: ${error.message}`;
            }

            alert(errorMessage);
            return false;
        }
    }

    createPeerConnection() {
        const configuration = {
            iceServers: [{
                urls: 'stun:stun.l.google.com:19302'
            }]
        };

        this.pc = new RTCPeerConnection(configuration);

        // Обработка ICE кандидатов
        this.pc.onicecandidate = event => {
            if (event.candidate) {
                console.log('Отправляем ICE кандидат');
                this.sendMessage({
                    type: 'candidate',
                    candidate: event.candidate
                });
            }
        };

        // Обработка входящего потока
        this.pc.ontrack = event => {
            this.remoteStream = event.streams[0];
            console.log('Получен удаленный поток');

            // Отображаем удаленное видео
            this.remoteVideo.srcObject = this.remoteStream;
            this.remoteVideoContainer.classList.add('active');
            this.remoteVideo.play().catch(e => console.log('Ошибка воспроизведения удаленного видео:', e));

            if (this.remoteInfo) {
                this.remoteInfo.textContent = 'Собеседник подключен';
            }

            this.showStatus('Соединение установлено!', 'success');
        };

        // Добавление локального потока
        if (this.localStream) {
            this.localStream.getTracks().forEach(track => {
                this.pc.addTrack(track, this.localStream);
            });
        }

        console.log('PeerConnection создан');
    }

    sendMessage(message) {
        if (this.drone && this.room) {
            console.log('Отправляем сообщение:', message);
            this.drone.publish({
                room: this.roomName,
                message: message
            });
        }
    }

    async startCall() {
        console.log('Начинаем видеозвонок... Инициатор:', this.isInitiator);

        this.showStatus('Устанавливаем соединение...', 'connecting');

        const mediaStarted = await this.startCameraAndMicrophone();
        if (!mediaStarted) {
            this.showStatus('Не удалось подключить камеру и микрофон', 'error');
            return;
        }

        // Создаем PeerConnection
        this.createPeerConnection();

        // Если мы инициатор - создаем предложение
        if (this.isInitiator) {
            console.log('Я инициатор, создаю предложение через 1 секунду');
            setTimeout(() => this.createOffer(), 1000);
        } else {
            console.log('Я отвечающий, жду предложения');
        }

        this.isCallActive = true;
        this.enableControls();
    }

    enableControls() {
        if (this.muteButton) this.muteButton.disabled = false;
        if (this.videoButton) this.videoButton.disabled = false;
        if (this.hangupButton) {
            this.hangupButton.style.display = 'block';
            this.hangupButton.disabled = false;
        }
    }

    async createOffer() {
        try {
            console.log('Создаем предложение...');
            const offer = await this.pc.createOffer();
            await this.pc.setLocalDescription(offer);

            this.sendMessage({
                type: 'offer',
                offer: offer
            });

            this.showStatus('Ожидание ответа...', 'connecting');
            console.log('Предложение отправлено');

        } catch (error) {
            console.error('Ошибка создания предложения:', error);
            this.showStatus('Ошибка установки соединения', 'error');
        }
    }

    async handleSignalingMessage(data, client) {
        console.log('Обрабатываем сообщение типа:', data.type);

        switch (data.type) {
            case 'offer':
                this.handleOffer(data, client);
                break;
            case 'answer':
                this.handleAnswer(data, client);
                break;
            case 'candidate':
                this.handleCandidate(data, client);
                break;
            default:
                console.log('Неизвестный тип сообщения:', data.type);
        }
    }

    async handleOffer(data, client) {
        console.log('Получено входящее предложение');

        try {
            await this.pc.setRemoteDescription(new RTCSessionDescription(data.offer));
            console.log('Установлено удаленное описание');

            // Обрабатываем отложенные ICE кандидаты
            this.processPendingCandidates();

            // Создаем и отправляем ответ
            const answer = await this.pc.createAnswer();
            await this.pc.setLocalDescription(answer);
            console.log('Создан и установлен локальный ответ');

            this.sendMessage({
                type: 'answer',
                answer: answer
            });

            this.isCallActive = true;
            this.enableControls();
            this.showStatus('Соединение устанавливается...', 'connecting');
            console.log('Ответ отправлен');

        } catch (error) {
            console.error('Ошибка обработки предложения:', error);
            this.showStatus('Ошибка установки соединения', 'error');
        }
    }

    async handleAnswer(data, client) {
        console.log('Получен ответ на звонок');

        try {
            await this.pc.setRemoteDescription(new RTCSessionDescription(data.answer));
            console.log('Установлен ответ от собеседника');

            // Обрабатываем отложенные ICE кандидаты
            this.processPendingCandidates();

            this.showStatus('Соединение установлено!', 'success');
        } catch (error) {
            console.error('Ошибка установки ответа:', error);
            this.showStatus('Ошибка установки соединения', 'error');
        }
    }

    async handleCandidate(data, client) {
        console.log('Получен ICE кандидат');
        try {
            if (this.pc) {
                // Проверяем, установлено ли удаленное описание
                if (this.pc.remoteDescription && this.pc.remoteDescription.type) {
                    await this.pc.addIceCandidate(new RTCIceCandidate(data.candidate));
                    console.log('ICE кандидат добавлен');
                } else {
                    // Если описание еще не установлено, сохраняем кандидат в буфер
                    console.log('Сохраняем ICE кандидат в буфер (ожидание remote description)');
                    this.pendingCandidates.push(data.candidate);
                }
            }
        } catch (error) {
            console.error('Ошибка добавления ICE кандидата:', error);
        }
    }

    processPendingCandidates() {
        if (this.pendingCandidates.length > 0) {
            console.log(`Обрабатываем ${this.pendingCandidates.length} ожидающих ICE кандидатов`);
            this.pendingCandidates.forEach(async (candidate) => {
                try {
                    await this.pc.addIceCandidate(new RTCIceCandidate(candidate));
                    console.log('Отложенный ICE кандидат добавлен');
                } catch (error) {
                    console.error('Ошибка добавления отложенного ICE кандидата:', error);
                }
            });
            this.pendingCandidates = [];
        }
    }

    hangUp() {
        console.log('Завершаем видеозвонок...');

        this.isCallActive = false;
        this.isAudioMuted = false;
        this.isVideoMuted = false;
        this.isInitiator = false;

        if (this.pc) {
            this.pc.close();
            this.pc = null;
        }

        if (this.localStream) {
            this.localStream.getTracks().forEach(track => track.stop());
            this.localStream = null;
        }

        if (this.remoteStream) {
            this.remoteStream = null;
        }

        // Скрываем видео
        this.localVideo.srcObject = null;
        this.remoteVideo.srcObject = null;
        this.localVideoContainer.classList.remove('active');
        this.remoteVideoContainer.classList.remove('active');

        this.resetUI();

        if (this.localInfo) {
            this.localInfo.textContent = 'Камера выключена';
        }
        if (this.remoteInfo) {
            this.remoteInfo.textContent = 'Ожидание подключения...';
        }

        if (this.localVideoOverlay) {
            this.localVideoOverlay.querySelector('.video-status').textContent = 'Камера выключена';
        }
        if (this.remoteVideoOverlay) {
            this.remoteVideoOverlay.querySelector('.video-status').textContent = 'Ожидание подключения...';
        }

        this.showStatus('Ожидание подключения собеседника...', 'connecting');
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

        if (this.muteButton) {
            this.muteButton.disabled = true;
            this.muteButton.innerHTML = '🔇 Вкл/Выкл звук';
        }

        if (this.videoButton) {
            this.videoButton.disabled = true;
            this.videoButton.innerHTML = '🎥 Вкл/Выкл видео';
        }
    }

    showStatus(message, type = 'info') {
        if (!this.statusContainer || !this.statusMessage) return;

        this.statusContainer.style.display = 'block';
        this.statusMessage.textContent = message;

        this.statusMessage.className = ''; // Очистить классы

        switch(type) {
            case 'connecting':
                this.statusMessage.classList.add('status-connecting');
                break;
            case 'success':
                this.statusMessage.classList.add('status-success');
                break;
            case 'error':
                this.statusMessage.classList.add('status-error');
                break;
            case 'incoming':
                this.statusMessage.classList.add('status-incoming');
                break;
        }

        console.log(`Статус: ${message}`);
    }

    toggleMute() {
        if (!this.localStream) return;

        const audioTracks = this.localStream.getAudioTracks();
        if (audioTracks.length > 0) {
            this.isAudioMuted = !this.isAudioMuted;
            audioTracks[0].enabled = !this.isAudioMuted;

            console.log(`Микрофон: ${this.isAudioMuted ? 'отключен' : 'включен'}`);

            if (this.muteButton) {
                this.muteButton.innerHTML = this.isAudioMuted ?
                    '🔊 Включить звук' : '🔇 Выключить звук';
            }

            if (this.localInfo) {
                const videoStatus = this.isVideoMuted ? 'видео отключено' : 'видео включено';
                this.localInfo.textContent = this.isAudioMuted ?
                    `${videoStatus}, звук отключен` : `${videoStatus}, звук включен`;
            }
        }
    }

    toggleVideo() {
        if (!this.localStream) return;

        const videoTracks = this.localStream.getVideoTracks();
        if (videoTracks.length > 0) {
            this.isVideoMuted = !this.isVideoMuted;
            videoTracks[0].enabled = !this.isVideoMuted;

            console.log(`Камера: ${this.isVideoMuted ? 'отключена' : 'включена'}`);

            if (this.videoButton) {
                this.videoButton.innerHTML = this.isVideoMuted ?
                    '📷 Включить видео' : '🎥 Выключить видео';
            }

            if (this.localInfo) {
                const audioStatus = this.isAudioMuted ? 'звук отключен' : 'звук включен';
                this.localInfo.textContent = this.isVideoMuted ?
                    `Видео отключено, ${audioStatus}` : `Видео включено, ${audioStatus}`;
            }

            // Обновляем отображение
            if (this.isVideoMuted) {
                this.localVideoContainer.classList.remove('active');
            } else {
                this.localVideoContainer.classList.add('active');
            }
        }
    }
}

// Инициализация после загрузки DOM
document.addEventListener('DOMContentLoaded', () => {
    try {
        const videoCall = new VideoCall();
        console.log('Видеозвонок инициализирован');
    } catch (error) {
        console.error('Ошибка инициализации видеозвонка:', error);
        alert('Ошибка загрузки приложения. Пожалуйста, обновите страницу.');
    }
});