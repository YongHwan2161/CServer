// uiHandler.js
import { currentIndex, maxIndex, update_current_index} from './app.js';
import { sendMessage } from './websocket.js';
import { getMessageByIndex,getForward2Messages, getBackward2Messages, modifyMessageByIndex, addMessageLink, removeMessageLink, getMessageLinks } from './api.js';

export function initUIHandlers() {
    document.getElementById('myButton').addEventListener('click', sendTextToServer);
    document.getElementById('getMessageButton').addEventListener('click', handleGetMessage);
    document.getElementById('modifyMessageButton').addEventListener('click', handleModifyMessage);
    document.getElementById('addLinkButton').addEventListener('click', handleAddLink);
    document.getElementById('removeLinkButton').addEventListener('click', handleRemoveLink);
    document.getElementById('getLinksButton').addEventListener('click', handleGetLinks);
    document.getElementById('upButton').addEventListener('click', () => changeIndex(1));
    document.getElementById('downButton').addEventListener('click', () => changeIndex(-1));
    
    const textInput = document.getElementById('textinput');
    textInput.addEventListener('keypress', function (event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            sendTextToServer();
        }
    });
}

export function updateOutput(content) {
    const output = document.getElementById('output');
    if (output) {
        output.innerHTML += content + '<br>';
        output.scrollTop = output.scrollHeight;
    }
}

export function updateCurrentMessage(message) {
    const currentContainer = document.getElementById('current-message');
    currentContainer.textContent = message;
}

export function updateForwardMessages(forwardLinks) {
    console.log('Updating forward messages');
    const forwardContainer = document.getElementById('forward-messages');
    forwardContainer.innerHTML = '';
    document.getElementById('forward-2-messages').innerHTML = '';

    forwardLinks.forEach((link, index) => {
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${index + 1}</span> ${getLinkContent(link)}`;
        forwardContainer.appendChild(linkElement);

        linkElement.addEventListener('click', function () {
            console.log('Clicked forward message');
            clearMessageContainer();
            update_current_index(parseInt(this.dataset.index));
            // currentIndex = parseInt(this.dataset.index);
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });

        getForward2Messages(link.index, index + 1);
    });
}

export function updateBackwardMessages(backwardLinks) {
    const backwardContainer = document.getElementById('backward-messages');
    backwardContainer.innerHTML = '';
    document.getElementById('backward-2-messages').innerHTML = '';

    backwardLinks.forEach((link, index) => {
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${-(index + 1)}</span> ${getLinkContent(link)}`;
        backwardContainer.appendChild(linkElement);

        linkElement.addEventListener('click', function () {
            console.log('Clicked backward message');
            clearMessageContainer();
            update_current_index(parseInt(this.dataset.index));
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });

        getBackward2Messages(link.index, index + 1);
    });
}

export function updateForward2Messages(forward2Links, parentNumber) {
    console.log('Updating forward2 messages', parentNumber);
    const forward2Container = document.getElementById('forward-2-messages');

    forward2Links.forEach((link, index) => {
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${parentNumber}.${index + 1}</span> ${getLinkContent(link)}`;
        forward2Container.appendChild(linkElement);

        linkElement.addEventListener('click', function () {
            console.log('Clicked forward2 message');
            clearMessageContainer();
            update_current_index(parseInt(this.dataset.index));
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });
    });
}
export function updateBackward2Messages(backward2Links, parentNumber) {
    console.log('Updating backward2 messages', parentNumber);
    const forward2Container = document.getElementById('backward-2-messages');

    backward2Links.forEach((link, index) => {
        const linkElement = document.createElement('div');
        linkElement.className = 'message-link';
        linkElement.dataset.index = link.index;
        linkElement.innerHTML = `<span class="link-number">${parentNumber}.${index + 1}</span> ${getLinkContent(link)}`;
        forward2Container.appendChild(linkElement);

        linkElement.addEventListener('click', function () {
            console.log('Clicked backward2 message');
            clearMessageContainer();
            update_current_index(parseInt(this.dataset.index));
            getMessageByIndex(currentIndex);
            updateIndexDisplay();
        });
    });
}

export function clearMessageContainer() {
    document.getElementById('forward-messages').innerHTML = '';
    document.getElementById('forward-2-messages').innerHTML = '';
    document.getElementById('backward-messages').innerHTML = '';
    document.getElementById('backward-2-messages').innerHTML = '';
}

export function updateIndexDisplay() {
    document.getElementById('currentIndex').textContent = currentIndex + '/' + maxIndex;
}

function sendTextToServer() {
    const textInput = document.getElementById('textinput');
    const message = textInput.value;
    sendMessage('message', `${message}|${currentIndex}`);
    updateOutput('Sent: ' + message);
    textInput.value = '';
    clearMessageContainer();
    getMessageByIndex(currentIndex);
    updateIndexDisplay();
}

function handleGetMessage() {
    const index = document.getElementById('messageIndexInput').value;
    const format = document.getElementById('outputFormatSelect').value;
    update_current_index(index);
    getMessageByIndex(index, format);
    updateIndexDisplay();
}

function handleModifyMessage() {
    const index = document.getElementById('modifyIndexInput').value;
    const newMessage = document.getElementById('modifyMessageInput').value;
    modifyMessageByIndex(index, newMessage);
}

function handleAddLink() {
    const sourceIndex = document.getElementById('sourceLinkInput').value;
    const targetIndex = document.getElementById('targetLinkInput').value;
    const direction = document.getElementById('linkDirectionSelect').value;
    addMessageLink(sourceIndex, targetIndex, direction);
}

function handleRemoveLink() {
    const sourceIndex = document.getElementById('sourceUnlinkInput').value;
    const targetIndex = document.getElementById('targetUnlinkInput').value;
    const direction = document.getElementById('unlinkDirectionSelect').value;
    removeMessageLink(sourceIndex, targetIndex, direction);
}

function handleGetLinks() {
    const index = document.getElementById('getLinksInput').value;
    const direction = document.getElementById('getLinkDirectionSelect').value;
    getMessageLinks(index, direction);
}

function changeIndex(delta) {
    update_current_index(currentIndex + delta);
    //currentIndex += delta;
    if (currentIndex < 1) currentIndex = 1;
    if (currentIndex > maxIndex) currentIndex = maxIndex;
    updateIndexDisplay(currentIndex, maxIndex);
    getMessageByIndex(currentIndex);
}
function getLinkContent(link) {
    if (typeof link === 'object') {
        return link.content || link.message || link.text || JSON.stringify(link);
    } else {
        return link;
    }
}
