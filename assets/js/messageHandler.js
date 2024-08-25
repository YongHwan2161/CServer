// messageHandler.js
import { updateOutput, updateCurrentMessage, updateForwardMessages, updateBackwardMessages, updateForward2Messages, updateBackward2Messages, clearMessageContainer, updateIndexDisplay } from './uiHandler.js';
import { currentIndex, maxIndex, updateMaxIndex } from './app.js';
import { getMessageByIndex } from './api.js';

export function initMessageHandlers() {
    document.addEventListener('websocketMessage', handleWebSocketMessage);
}

function handleWebSocketMessage(event) {
    const data = event.detail;
    switch (data.action) {
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
}

function handleMessageResponse(data) {
    if (data.content && !data.links) {
        updateCurrentMessage(data.content);
    }
    if (data.links) {
        if (data.links.forward) {
            updateForwardMessages(data.links.forward);
        }
        if (data.links.backward) {
            updateBackwardMessages(data.links.backward);
        }
        if (data.links.forward2) {
            updateForward2Messages(data.links.forward2, data.parentNumber);
        }
        if (data.links.backward2) {
            updateBackward2Messages(data.links.backward2, data.parentNumber);
        }
    }
    if (data.saved_index && data.max_index) {
        if (maxIndex != data.max_index) {
            updateMaxIndex(data.max_index);
        }
        updateIndexDisplay(currentIndex, maxIndex);
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

function displayIndexTableInfo(data) {
    // Implementation remains the same
}

function displayFreeSpaceTableInfo(data) {
    // Implementation remains the same
}

function handleMaxIndex(value) {
    updateMaxIndex(value);
    updateIndexDisplay();
    console.log('Max index:', maxIndex);
    getMessageByIndex(currentIndex);
    clearMessageContainer();
}