// WebSocket 연결 설정
let socket;

function connectWebSocket() {
    socket = new WebSocket('wss://' + window.location.host + '/websocket');

    socket.onopen = function (event) {
        console.log('WebSocket connected');
        listFiles('');
    };

    socket.onmessage = function (event) {
        const data = JSON.parse(event.data);
        switch (data.action) {
            case 'file_list':
                updateFileTree(data.items, data.path);
                break;
            case 'file_content':
                updateEditor(data.content);
                break;
            case 'save_result':
            case 'build_result':
            case 'run_output':
                updateOutput(data.content);
                break;
            case 'message_response':
                handleServerResponse(data.content);
                break;
            default:
                console.log('Unknown action:', data.action);
        }
    };

    socket.onclose = function (event) {
        console.log('WebSocket disconnected');
        setTimeout(connectWebSocket, 3000);
    };
}

// 함수들을 모듈 스코프에서 정의
function sendTextToServer() {
    const textInput = document.getElementById('textinput');
    const message = textInput.value;
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: message }));
        updateOutput('Sent: ' + message);
        textInput.value = '';
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function handleServerResponse(content) {
    updateOutput('Received: ' + content);
}

// 새로운 함수: 특정 인덱스의 메시지를 요청
function getMessageByIndex() {
    const indexInput = document.getElementById('indexInput');
    const index = indexInput.value;
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: 'get:' + index }));
        updateOutput('Requesting message with index: ' + index);
        indexInput.value = '';
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}
// 새로운 함수: 특정 인덱스의 메시지를 수정
function modifyMessageByIndex() {
    const indexInput = document.getElementById('modifyIndexInput');
    const messageInput = document.getElementById('modifyMessageInput');
    const index = indexInput.value;
    const newMessage = messageInput.value;
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `modify:${index}:${newMessage}` }));
        updateOutput(`Modifying message with index: ${index}`);
        indexInput.value = '';
        messageInput.value = '';
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function listFiles(path) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'list_files', path: path }));
    } else {
        console.error('WebSocket is not connected');
    }
}

function updateFileTree(items, path) {
    // 파일 트리 업데이트 로직을 구현하세요
    console.log('File tree updated', items, path);
}

function updateEditor(content) {
    // 에디터 업데이트 로직을 구현하세요
    console.log('Editor content updated', content);
}

function updateOutput(content) {
    const output = document.getElementById('output');
    if (output) {
        output.innerHTML += content + '<br>';
        output.scrollTop = output.scrollHeight;
    }
}
function updateStatus(message) {
    const status = document.getElementById('status');
    if (status) {
        status.textContent = message;
    }
}

window.onload = function () {
    console.log('Page loaded');
    connectWebSocket();

    const sendButton = document.getElementById('myButton');
    if (sendButton) {
        sendButton.addEventListener('click', sendTextToServer);
    }
};

// 초기화 함수
function init() {
    console.log('Page loaded');
    connectWebSocket();

    const sendButton = document.getElementById('myButton');
    if (sendButton) {
        sendButton.addEventListener('click', sendTextToServer);
    }
    const getMessageButton = document.getElementById('getMessageButton');
    if (getMessageButton) {
        getMessageButton.addEventListener('click', getMessageByIndex);
    }
    const modifyMessageButton = document.getElementById('modifyMessageButton');
    if (modifyMessageButton) {
        modifyMessageButton.addEventListener('click', modifyMessageByIndex);
    }

    const textInput = document.getElementById('textinput');
    if (textInput) {
        textInput.addEventListener('keypress', function(event) {
            if (event.key === 'Enter') {
                event.preventDefault();
                sendTextToServer();
            }
        });
    }
}
// DOMContentLoaded 이벤트에 초기화 함수 연결
document.addEventListener('DOMContentLoaded', init);

// 필요한 경우 함수를 내보냄
export { sendTextToServer, listFiles, getMessageByIndex, modifyMessageByIndex };