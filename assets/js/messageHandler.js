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
    const tableContainer = document.getElementById('tableContainer');
    tableContainer.innerHTML = '<h2>Index Table</h2>';
    
    if (data.length === 0) {
        tableContainer.innerHTML += '<p>No index data available.</p>';
        return;
    }

    const table = document.createElement('table');
    table.innerHTML = `
        <tr>
            <th>Index</th>
            <th>Offset</th>
            <th>Length</th>
        </tr>
    `;

    data.sort((a, b) => a.index - b.index);

    data.forEach(entry => {
        const row = table.insertRow();
        row.insertCell().textContent = entry.index;
        row.insertCell().textContent = entry.offset;
        row.insertCell().textContent = entry.length;
    });

    tableContainer.appendChild(table);
}

function displayFreeSpaceTableInfo(data) {
    const tableContainer = document.getElementById('tableContainer');
    tableContainer.innerHTML = '<h2>Free Space Table</h2>';
    
    if (data.length === 0) {
        tableContainer.innerHTML += '<p>No free space data available.</p>';
        return;
    }

    const table = document.createElement('table');
    table.innerHTML = `
        <tr>
            <th>Offset</th>
            <th>Length</th>
        </tr>
    `;

    data.sort((a, b) => a.offset - b.offset);

    data.forEach(entry => {
        const row = table.insertRow();
        row.insertCell().textContent = entry.offset;
        row.insertCell().textContent = entry.length;
    });

    tableContainer.appendChild(table);
}
// Helper function to create a sortable table
function createSortableTable(headers, data, sortFunction) {
    const table = document.createElement('table');
    const headerRow = table.createTHead().insertRow();
    
    headers.forEach(header => {
        const th = document.createElement('th');
        th.textContent = header;
        th.addEventListener('click', () => sortTable(table, headers.indexOf(header), sortFunction));
        headerRow.appendChild(th);
    });

    const tbody = table.createTBody();
    data.forEach(rowData => {
        const row = tbody.insertRow();
        rowData.forEach(cellData => {
            const cell = row.insertCell();
            cell.textContent = cellData;
        });
    });

    return table;
}

// Helper function to sort table
function sortTable(table, columnIndex, sortFunction) {
    const tbody = table.tBodies[0];
    const rows = Array.from(tbody.rows);

    rows.sort((a, b) => {
        const aValue = a.cells[columnIndex].textContent;
        const bValue = b.cells[columnIndex].textContent;
        return sortFunction(aValue, bValue);
    });

    rows.forEach(row => tbody.appendChild(row));
}

// Example sort functions
// const sortNumeric = (a, b) => Number(a) - Number(b);
// const sortAlphabetic = (a, b) => a.localeCompare(b);
function handleMaxIndex(value) {
    updateMaxIndex(value);
    updateIndexDisplay();
    console.log('Max index:', maxIndex);
    getMessageByIndex(currentIndex);
    clearMessageContainer();
}