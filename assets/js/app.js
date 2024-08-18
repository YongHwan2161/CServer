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
                    handleMessageResponse(data.content, data.format);
                    break;
            case 'index_table_info':
                displayIndexTableInfo(data.data);
                break;
            case 'free_space_table_info':
                displayFreeSpaceTableInfo(data.data);
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
// 수정된 함수: 메시지 또는 바이너리 데이터 요청
function getMessageByIndex() {
    const indexInput = document.getElementById('messageIndexInput');
    const formatSelect = document.getElementById('outputFormatSelect');
    const index = indexInput.value;
    const format = formatSelect.value;

    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `get:${index}:${format}` }));
        updateOutput(`Requesting message with index: ${index} in ${format} format`);
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
function getIndexTableInfo() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: 'get_index_table_info' }));
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function getFreeSpaceTableInfo() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: 'get_free_space_table_info' }));
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function displayIndexTableInfo(data) {
    let tableHTML = '<h2>Index Table Info</h2><table><tr><th>Index</th><th>Offset</th><th>Length</th></tr>';
    data.forEach(entry => {
        tableHTML += `<tr><td>${entry.index}</td><td>${entry.offset}</td><td>${entry.length}</td></tr>`;
    });
    tableHTML += '</table>';
    document.getElementById('tableContainer').innerHTML = tableHTML;
}

function displayFreeSpaceTableInfo(data) {
    let tableHTML = '<h2>Free Space Table Info</h2><table><tr><th>Offset</th><th>Length</th></tr>';
    data.forEach(entry => {
        tableHTML += `<tr><td>${entry.offset}</td><td>${entry.length}</td></tr>`;
    });
    tableHTML += '</table>';
    document.getElementById('tableContainer').innerHTML = tableHTML;
}
// 새로운 함수: 서버 응답 처리
function handleMessageResponse(content, format) {
    switch (format) {
        case 'text':
            updateOutput('Retrieved message: ' + content);
            break;
        case 'binary':
            const binaryString = hexToBinary(content);
            updateOutput('Binary data: ' + binaryString);
            break;
        case 'hex':
            updateOutput('Hexadecimal data: ' + content);
            break;
        default:
            updateOutput('Unknown format: ' + content);
    }
}

// 새로운 함수: 특정 인덱스의 바이너리 데이터를 요청
// function getBinaryDataByIndex() {
//     const indexInput = document.getElementById('binaryIndexInput');
//     const index = indexInput.value;
//     if (socket && socket.readyState === WebSocket.OPEN) {
//         socket.send(JSON.stringify({ action: 'message', content: 'get_binary:' + index }));
//         updateOutput('Requesting binary data for index: ' + index);
//         indexInput.value = '';
//     } else {
//         console.error('WebSocket is not connected');
//         updateStatus('Error: WebSocket is not connected');
//     }
// }
// // 새로운 함수: 바이너리 데이터를 화면에 표시
// function displayBinaryData(hexString) {
//     const binaryString = hexString.match(/.{1,2}/g).map(byte => parseInt(byte, 16).toString(2).padStart(8, '0')).join(' ');
//     updateOutput('Binary data: ' + binaryString);
// }
// 새로운 함수: 16진수를 이진수 문자열로 변환
function hexToBinary(hexString) {
    return hexString.match(/.{1,2}/g)
        .map(byte => parseInt(byte, 16).toString(2).padStart(8, '0'))
        .join(' ');
}

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
    const getIndexTableInfoButton = document.getElementById('getIndexTableInfoButton');
    if (getIndexTableInfoButton) {
        getIndexTableInfoButton.addEventListener('click', getIndexTableInfo);
    }

    const getFreeSpaceTableInfoButton = document.getElementById('getFreeSpaceTableInfoButton');
    if (getFreeSpaceTableInfoButton) {
        getFreeSpaceTableInfoButton.addEventListener('click', getFreeSpaceTableInfo);
    }
    // const getBinaryDataButton = document.getElementById('getBinaryDataButton');
    // if (getBinaryDataButton) {
    //     getBinaryDataButton.addEventListener('click', getBinaryDataByIndex);
    // }

    const textInput = document.getElementById('textinput');
    if (textInput) {
        textInput.addEventListener('keypress', function (event) {
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