// api.js
import { sendMessage } from './websocket.js';
import { updateOutput } from './uiHandler.js';

export function getMaxIndex() {
    return new Promise((resolve, reject) => {
        sendMessage('message', 'get_max_index');
        document.addEventListener('websocketMessage', function handler(event) {
            const data = event.detail;
            if (data.action === 'max_index') {
                document.removeEventListener('websocketMessage', handler);
                resolve(data.value);
            }
        });
    });
}

export function getMessageByIndex(index, format = 'text') {
    sendMessage('message', `get:${index}:${format}`);
    sendMessage('message', `get:${index}:${format}:forward`);
    sendMessage('message', `get:${index}:${format}:backward`);
    updateOutput(`Requesting message and links with index: ${index}`);
}
export function getForward2Messages(index, parent_index, format = 'text'){
    sendMessage('message', `get:${index}:${format}:forward2:${parent_index}`);
}
export function getBackward2Messages(index, parent_index, format = 'text'){
    sendMessage('message', `get:${index}:${format}:backward2:${parent_index}`);
}
export function modifyMessageByIndex(index, newMessage) {
    sendMessage('message', `modify:${index}:${newMessage}`);
    updateOutput(`Modifying message with index: ${index}`);
}

export function addMessageLink(sourceIndex, targetIndex, direction) {
    sendMessage('message', `link:${direction}:${sourceIndex}:${targetIndex}`);
    updateOutput(`Adding ${direction} link from message ${sourceIndex} to ${targetIndex}`);
}

export function removeMessageLink(sourceIndex, targetIndex, direction) {
    sendMessage('message', `unlink:${direction}:${sourceIndex}:${targetIndex}`);
    updateOutput(`Removing ${direction} link from message ${sourceIndex} to ${targetIndex}`);
}

export function getMessageLinks(index, direction) {
    sendMessage('message', `getlinks:${index}:${direction}`);
    updateOutput(`Getting ${direction} links for message ${index}`);
}

export function getIndexTableInfo() {
    sendMessage('message', 'get_index_table_info');
}

export function getFreeSpaceTableInfo() {
    sendMessage('message', 'get_free_space_table_info');
}