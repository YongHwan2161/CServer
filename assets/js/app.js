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
                console.log('case : message_response' + 'data = ' + data.content);
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
        // 메시지와 현재 인덱스를 '|' 문자로 구분하여 전송
        const content = `${message}|${currentIndex}`;
        socket.send(JSON.stringify({
            action: 'message',
            content: content
        }));
        updateOutput('Sent: ' + message);
        textInput.value = '';
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}

// 수정된 함수: 메시지 요청
function getMessageByIndex(index, format = 'text') {
    console.log('call getMessageByIndex');
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `get:${index}:${format}` }));
        socket.send(JSON.stringify({ action: 'message', content: `get:${index}:${format}:forward` }));
        socket.send(JSON.stringify({ action: 'message', content: `get:${index}:${format}:backward` }));
        updateOutput(`Requesting message and links with index: ${index}`);
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
function addMessageLink(sourceIndex, targetIndex, direction) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `link:${direction}:${sourceIndex}:${targetIndex}` }));
        updateOutput(`Adding ${direction} link from message ${sourceIndex} to ${targetIndex}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}
function removeMessageLink(sourceIndex, targetIndex, direction) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `unlink:${direction}:${sourceIndex}:${targetIndex}` }));
        updateOutput(`Removing ${direction} link from message ${sourceIndex} to ${targetIndex}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}
function getMessageLinks(index, direction) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `getlinks:${index}:${direction}` }));
        updateOutput(`Getting ${direction} links for message ${index}`);
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
    let tableHTML = '<h2>Index Table Info</h2><table><tr><th>Index</th><th>Offset</th><th>Length</th><th>Forward Links</th><th>Backward Links</th></tr>';
    data.forEach(entry => {
        let forwardLinksString = entry.forward_links.length > 0 ? entry.forward_links.join(", ") : "None";
        let backwardLinksString = entry.backward_links.length > 0 ? entry.backward_links.join(", ") : "None";
        tableHTML += `<tr><td>${entry.index}</td><td>${entry.offset}</td><td>${entry.length}</td><td>${forwardLinksString}</td><td>${backwardLinksString}</td></tr>`;
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
function getLinkContent(link) {
    if (typeof link === 'object') {
        // 객체인 경우, 가장 적절한 속성을 선택하여 표시
        return link.content || link.message || link.text || JSON.stringify(link);
    } else {
        // 객체가 아닌 경우 (예: 문자열, 숫자 등) 그대로 반환
        return link;
    }
}

function updateBackwardMessages(backwardLinks) {
    const backwardContainer = document.getElementById('backward-messages');
    backwardContainer.innerHTML = '';
    
    backwardLinks.forEach((link, index) => {
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${-(index + 1)}</span> ${getLinkContent(link)}`;
        backwardContainer.appendChild(linkElement);

        // Add click event listener
        linkElement.addEventListener('click', function () {
            console.log('click backwardMessage button');
            clearMessageContainer();
            currentIndex = parseInt(this.dataset.index);
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });
    });
}
function getForward2Messages(index, parentNumber) {
    console.log('call getForward2Messages');
    if (socket && socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify({ action: 'message', content: `get:${index}:text:forward2:${parentNumber}` }));
        updateOutput(`Requesting forward2 messages for index: ${index}`);
    } else {
        console.error('WebSocket is not connected');
        updateStatus('Error: WebSocket is not connected');
    }
}
function updateForward2Messages(forward2Links, parentNumber) {
    console.log('call updateForward2Messages' + ', parentNumber: ' + parentNumber);
    const forward2Container = document.getElementById('forward-2-messages');
    //forward2Container.innerHTML = '?';

    forward2Links.forEach((link, index) => {
        console.log('getLinkContent(link): ' + getLinkContent(link));
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${parentNumber}.${index + 1}</span> ${getLinkContent(link)}`;
        forward2Container.appendChild(linkElement);

        // Add click event listener
        linkElement.addEventListener('click', function () {
            console.log('click forward2Message button');
            clearMessageContainer();
            currentIndex = parseInt(this.dataset.index);
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });
    });
}

function updateForwardMessages(forwardLinks) {
    console.log('call updateForwardMessages');
    const forwardContainer = document.getElementById('forward-messages');
    // Clear previous content
    forwardContainer.innerHTML = '';
    document.getElementById('forward-2-messages').innerHTML = '';

    forwardLinks.forEach((link, index) => {
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${index + 1}</span> ${getLinkContent(link)}`;
        forwardContainer.appendChild(linkElement);

        // Add click event listener
        linkElement.addEventListener('click', function () {
            console.log('click forwardMessage button');
            clearMessageContainer();
            currentIndex = parseInt(this.dataset.index);
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });

        // Fetch forward2 messages for this link
        console.log('request forward2Messages to server');
        getForward2Messages(link.index, index + 1);
        //getMessageByIndex(link.index,'text', index + 1)
    });
}

function updateCurrentMessage(currentMessage) {
    const currentContainer = document.getElementById('current-message');
    currentContainer.textContent = currentMessage;
}

function handleMessageResponse(data) {
    if (data.content && !data.links) {
        updateCurrentMessage(data.content);
    }
    if (data.links) {
        if (data.links.forward) {
            console.log('data.links.forward: handleMessageResponse');
            updateForwardMessages(data.links.forward);
        }
        if (data.links.backward) {
            updateBackwardMessages(data.links.backward);
        }
        // Handle forward2 messages
        if (data.links.forward2) {
            console.log('receive forward2 from server');
            updateForward2Messages(data.links.forward2, data.parentNumber);
        }
    }
    if (data.saved_index && data.max_index) {
        if (maxIndex != data.max_index) {
            maxIndex = data.max_index;
        }
        updateIndexDisplay();
        if (data.content === "Message saved successfully") {
            if (data.linked_index) {
                updateOutput(`New message (index ${data.saved_index}) linked to message ${data.linked_index}`);
                getMessageByIndex(currentIndex);
            }
        } else {
            currentIndex = data.saved_index;
        }
    }
    updateOutput(JSON.stringify(data));
}
function updateIndexDisplay() {
    document.getElementById('currentIndex').textContent = currentIndex + '/' + maxIndex;
}

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
function clearMessageContainer(){
    document.getElementById('forward-messages').innerHTML = '';
    document.getElementById('forward-2-messages').innerHTML = '';
    document.getElementById('backward-messages').innerHTML = '';
}
// 초기화 함수
function init() {
    console.log('Page loaded');
    connectWebSocket();

    const sendButton = document.getElementById('myButton');
    if (sendButton) {
        sendButton.addEventListener('click', function () {
            sendTextToServer();
            clearMessageContainer();
        });
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
            const direction = document.getElementById('linkDirectionSelect').value;
            addMessageLink(sourceIndex, targetIndex, direction);
        });
    }

    const removeLinkButton = document.getElementById('removeLinkButton');
    if (removeLinkButton) {
        removeLinkButton.addEventListener('click', () => {
            const sourceIndex = document.getElementById('sourceUnlinkInput').value;
            const targetIndex = document.getElementById('targetUnlinkInput').value;
            const direction = document.getElementById('unlinkDirectionSelect').value;
            removeMessageLink(sourceIndex, targetIndex, direction);
        });
    }

    const getLinksButton = document.getElementById('getLinksButton');
    if (getLinksButton) {
        getLinksButton.addEventListener('click', () => {
            const index = document.getElementById('getLinksInput').value;
            const direction = document.getElementById('getLinkDirectionSelect').value;
            getMessageLinks(index, direction);
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