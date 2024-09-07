// websocket.js
let socket;

export function initWebSocket() {
    return new Promise((resolve, reject) => {
        socket = new WebSocket('wss://' + window.location.host + '/websocket');

        socket.onopen = function (event) {
            console.log('WebSocket connected');
            resolve();
        };

        // socket.onmessage = function (event) {
        //     const data = JSON.parse(event.data);
        //     document.dispatchEvent(new CustomEvent('websocketMessage', { detail: data }));
        // };
        socket.onmessage = function (event) {
            try {
                // 수신된 데이터가 비어있지 않은지 확인
                if (event.data.trim() === '') {
                    console.warn('Received empty message from server');
                    return;
                }
                
                const data = JSON.parse(event.data);
                document.dispatchEvent(new CustomEvent('websocketMessage', { detail: data }));
            } catch (error) {
                console.error('Error parsing WebSocket message:', error);
                console.error('Received data:', event.data);
                // 오류 발생 시 사용자에게 알림
                document.dispatchEvent(new CustomEvent('websocketError', { 
                    detail: { message: 'Failed to parse server response', originalData: event.data } 
                }));
            }
        };
        socket.onclose = function (event) {
            console.log('WebSocket disconnected');
            setTimeout(initWebSocket, 3000);
        };
        socket.onerror = function (error) {
            console.error('WebSocket error:', error);
            reject(error);
        };
    });
}

export function sendMessage(action, content) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action, content }));
    } else {
        console.error('WebSocket is not connected');
    }
}