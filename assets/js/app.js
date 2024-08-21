// WebSocket 연결 설정
let socket;
let currentIndex = 1;
let maxIndex = 1;

function connectWebSocket() {
    socket = new WebSocket('wss://' + window.location.host + '/websocket');

    socket.onopen = function (event) {
        console.log('WebSocket connected');
        // listFiles('');
        // 최대 인덱스 요청
        getMaxIndex();
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
                handleMessageResponse(data);
                break;
            case 'index_table_info':
                displayIndexTableInfo(data.data);
                break;
            case 'free_space_table_info':
                displayFreeSpaceTableInfo(data.data);
                break;
            case 'max_index':
                handleMaxIndex(data.value);
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

// 수정된 함수: 메시지 요청
function getMessageByIndex(index, format = 'text') {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `get:${index}:${format}:withlink` }));
        updateOutput(`Requesting message with index: ${index} in ${format} format (with link)`);
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

function addMessageLink(sourceIndex, targetIndex) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `link:${sourceIndex}:${targetIndex}` }));
        updateOutput(`Adding link from message ${sourceIndex} to ${targetIndex}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function removeMessageLink(sourceIndex, targetIndex) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `unlink:${sourceIndex}:${targetIndex}` }));
        updateOutput(`Removing link from message ${sourceIndex} to ${targetIndex}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function getMessageLinks(index) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `getlinks:${index}` }));
        updateOutput(`Getting links for message ${index}`);
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
    let tableHTML = '<h2>Index Table Info</h2><table><tr><th>Index</th><th>Offset</th><th>Length</th><th>Links</th></tr>';
    data.forEach(entry => {
        let linksString = entry.links.length > 0 ? entry.links.join(", ") : "None";
        tableHTML += `<tr><td>${entry.index}</td><td>${entry.offset}</td><td>${entry.length}</td><td>${linksString}</td></tr>`;
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
function updateMessageDisplay(message, isLinkedMessage = false) {
    const container = isLinkedMessage ? document.getElementById('linked-message-container') : document.getElementById('message-container');
    if (!container) {
        console.error('Message container not found');
        return;
    }
    container.textContent = message;
}
function clearLinkedMessagesDisplay() {
    const container = document.getElementById('linked-message-container');
    if (container) {
        container.innerHTML = 'No linked messages';
    }
}
function handleMessageResponse(data) {
    updateOutput(data.content);
    if (data.saved_index && data.max_index) {
        if (maxIndex != data.max_index) {
            maxIndex = data.max_index;
        }
        updateIndexDisplay();
        
        // 새 메시지가 저장된 경우 (currentIndex가 변경되지 않음)
        if (data.content === "Message saved successfully") {
            // message-container를 업데이트하지 않음
        } else {
            // 기존 메시지를 가져온 경우
            currentIndex = data.saved_index;
            updateMessageDisplay(data.content);
        }
    } else if (data.content.startsWith("Message with index") && data.content.includes("modified successfully")) {
        // 메시지 수정 성공 시 message-container를 업데이트하지 않음
    } else {
        // 그 외의 경우 (예: 메시지 조회)
        updateMessageDisplay(data.content);
    }
    if (data.links && data.links.length > 0) {
        updateLinkedMessagesDisplay(data.links);
    } else {
        clearLinkedMessagesDisplay();
    }
}
function updateLinkedMessagesDisplay(links) {
    const container = document.getElementById('linked-message-container');
    if (!container) {
        console.error('Linked messages container not found');
        return;
    }
    container.innerHTML = '';
    if (links.length === 0) {
        container.textContent = "No linked messages";
    } else {
        const ul = document.createElement('ul');
        links.forEach(link => {
            const li = document.createElement('li');
            li.innerHTML = `<strong>Message ${link.index}:</strong> ${link.content}`;
            li.onclick = () => {
                getMessageByIndex(link.index);
                currentIndex = link.index;
                updateIndexDisplay();
            };
            ul.appendChild(li);
        });
        container.appendChild(ul);
    }
}

function updateIndexDisplay() {
    document.getElementById('currentIndex').textContent = currentIndex + '/' + maxIndex;
}
// 새로운 함수: 링크된 메시지 처리
// function handleLinkedMessage(linkedIndex) {
//     if (linkedIndex > 0) {
//         getMessageByIndex(linkedIndex);
//     } else {
//         updateMessageDisplay("No linked message", true);
//     }
// }

// 새로운 함수: 최대 인덱스 요청
function getMaxIndex() {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: 'get_max_index' }));
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}
// 새로운 함수: 최대 인덱스 처리
function handleMaxIndex(value) {
    maxIndex = value;
    document.getElementById('currentIndex').textContent = currentIndex + '/' + maxIndex;
    console.log('Max index:', maxIndex);
    // 최대 인덱스를 받은 후 첫 번째 메시지 요청
    getMessageByIndex(currentIndex);
}
// 수정된 함수: 인덱스 변경
function changeIndex(delta) {
    currentIndex += delta;
    if (currentIndex < 1) currentIndex = 1;
    if (currentIndex > maxIndex) currentIndex = maxIndex;
    updateIndexDisplay();
    getMessageByIndex(currentIndex);
}
function setMessageLink(sourceIndex, targetIndex) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `link:${sourceIndex}:${targetIndex}` }));
        updateOutput(`Setting link from message ${sourceIndex} to ${targetIndex}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

function getMessageLink(index) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `getlink:${index}` }));
        updateOutput(`Getting link for message ${index}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
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
        getMessageButton.addEventListener('click', () => {
            const index = document.getElementById('messageIndexInput').value;
            const format = document.getElementById('outputFormatSelect').value;
            getMessageByIndex(index, format);
        });
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
    const addLinkButton = document.getElementById('addLinkButton');
    if (addLinkButton) {
        addLinkButton.addEventListener('click', () => {
            const sourceIndex = document.getElementById('sourceLinkInput').value;
            const targetIndex = document.getElementById('targetLinkInput').value;
            addMessageLink(sourceIndex, targetIndex);
        });
    }

    const removeLinkButton = document.getElementById('removeLinkButton');
    if (removeLinkButton) {
        removeLinkButton.addEventListener('click', () => {
            const sourceIndex = document.getElementById('sourceUnlinkInput').value;
            const targetIndex = document.getElementById('targetUnlinkInput').value;
            removeMessageLink(sourceIndex, targetIndex);
        });
    }

    const getLinksButton = document.getElementById('getLinksButton');
    if (getLinksButton) {
        getLinksButton.addEventListener('click', () => {
            const index = document.getElementById('getLinksInput').value;
            getMessageLinks(index);
        });
    }
    const upButton = document.getElementById('upButton');
    if (upButton) {
        upButton.addEventListener('click', () => changeIndex(1));
    }

    const downButton = document.getElementById('downButton');
    if (downButton) {
        downButton.addEventListener('click', () => changeIndex(-1));
    }

    const textInput = document.getElementById('textinput');
    if (textInput) {
        textInput.addEventListener('keypress', function (event) {
            if (event.key === 'Enter') {
                event.preventDefault();
                sendTextToServer();
            }
        });
    }
    const setLinkButton = document.getElementById('setLinkButton');
    if (setLinkButton) {
        setLinkButton.addEventListener('click', () => {
            const sourceIndex = document.getElementById('sourceLinkInput').value;
            const targetIndex = document.getElementById('targetLinkInput').value;
            setMessageLink(sourceIndex, targetIndex);
        });
    }

    const getLinkButton = document.getElementById('getLinkButton');
    if (getLinkButton) {
        getLinkButton.addEventListener('click', () => {
            const index = document.getElementById('getLinkInput').value;
            getMessageLink(index);
        });
    }
    updateIndexDisplay();
}
// DOMContentLoaded 이벤트에 초기화 함수 연결
document.addEventListener('DOMContentLoaded', init);

// 필요한 경우 함수를 내보냄
export { sendTextToServer, listFiles, getMessageByIndex, modifyMessageByIndex };