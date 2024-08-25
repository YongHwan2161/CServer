import { initWebSocket, sendMessage } from './websocket.js';
import { initMessageHandlers } from './messageHandler.js';
import { initUIHandlers, updateIndexDisplay } from './uiHandler.js';
import { getMaxIndex } from './api.js';

let currentIndex = 1;
let maxIndex = 1;

async function init() {
    console.log('Page loaded');
    await initWebSocket();
    initMessageHandlers();
    initUIHandlers();
    
    maxIndex = await getMaxIndex();
    updateIndexDisplay(currentIndex, maxIndex);
}
export function updateMaxIndex(newMaxIndex) {
    maxIndex = newMaxIndex;
}
export function update_current_index(delta){
    currentIndex = delta;
}
document.addEventListener('DOMContentLoaded', init);

export { currentIndex, maxIndex };