// websocket.js
let socket;

export function initWebSocket() {
    return new Promise((resolve, reject) => {
        socket = new WebSocket('wss://' + window.location.host + '/websocket');

        socket.onopen = function (event) {
            console.log('WebSocket connected');
            resolve();
        };

        socket.onmessage = function (event) {
            const data = JSON.parse(event.data);
            document.dispatchEvent(new CustomEvent('websocketMessage', { detail: data }));
        };

        socket.onclose = function (event) {
            console.log('WebSocket disconnected');
            setTimeout(initWebSocket, 3000);
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