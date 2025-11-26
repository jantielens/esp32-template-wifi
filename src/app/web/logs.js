/**
 * Device Logs Viewer JavaScript
 * Displays real-time device logs using polling
 */

const API_LOGS = '/api/logs';
let logsPollingTimeout = null;
let logsAutoScroll = true;
let logsConnected = false;
let lastLogCount = 0;
let isFetching = false;

// Format timestamp to readable string (HH:MM:SS.milliseconds)
function formatLogTimestamp(ms) {
    const totalSeconds = Math.floor(ms / 1000);
    const hours = Math.floor(totalSeconds / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    const millis = ms % 1000;
    
    return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${String(millis).padStart(3, '0')}`;
}

// Add log entry to viewer
function addLogEntry(timestamp, message) {
    const viewer = document.getElementById('logs-viewer');
    
    // Remove placeholder if present
    const placeholder = viewer.querySelector('.logs-placeholder');
    if (placeholder) {
        placeholder.remove();
    }
    
    // Create log entry element
    const entry = document.createElement('div');
    entry.className = 'log-entry';
    
    const timeSpan = document.createElement('span');
    timeSpan.className = 'log-timestamp';
    timeSpan.textContent = formatLogTimestamp(timestamp);
    
    const msgSpan = document.createElement('span');
    msgSpan.className = 'log-message';
    msgSpan.textContent = message;
    
    entry.appendChild(timeSpan);
    entry.appendChild(msgSpan);
    viewer.appendChild(entry);
    
    // Auto-scroll if enabled
    if (logsAutoScroll) {
        viewer.scrollTop = viewer.scrollHeight;
    }
    
    // Limit to 500 entries to prevent memory issues
    const entries = viewer.querySelectorAll('.log-entry');
    if (entries.length > 500) {
        entries[0].remove();
    }
}

// Fetch and display logs from server
async function fetchLogs() {
    // Skip if already fetching
    if (isFetching) {
        return;
    }
    
    isFetching = true;
    
    try {
        const response = await fetch(API_LOGS);
        if (!response.ok) {
            throw new Error('Failed to fetch logs');
        }
        
        const data = await response.json();
        
        // Always update display (removed count check)
        const viewer = document.getElementById('logs-viewer');
        viewer.innerHTML = ''; // Clear existing
        
        data.logs.forEach(log => {
            addLogEntry(log.ts, log.msg);
        });
        
        lastLogCount = data.count;
    } catch (error) {
        console.error('Error fetching logs:', error);
    } finally {
        isFetching = false;
        
        // Schedule next fetch only if still connected
        if (logsConnected) {
            logsPollingTimeout = setTimeout(fetchLogs, 2000);
        }
    }
}

// Start log polling
function startLogPolling() {
    if (logsConnected) {
        return; // Already polling
    }
    
    const statusElem = document.getElementById('logs-status');
    statusElem.textContent = 'Connected';
    statusElem.className = 'logs-status connected';
    
    logsConnected = true;
    document.getElementById('logs-connect-btn').style.display = 'none';
    document.getElementById('logs-disconnect-btn').style.display = 'inline-block';
    
    // Start polling chain
    fetchLogs();
}

// Stop log polling
function stopLogPolling() {
    logsConnected = false;
    
    if (logsPollingTimeout) {
        clearTimeout(logsPollingTimeout);
        logsPollingTimeout = null;
    }
    
    const statusElem = document.getElementById('logs-status');
    statusElem.textContent = 'Disconnected';
    statusElem.className = 'logs-status disconnected';
    document.getElementById('logs-connect-btn').style.display = 'inline-block';
    document.getElementById('logs-disconnect-btn').style.display = 'none';
}

// Initialize logs viewer
function initLogsViewer() {
    document.getElementById('logs-connect-btn').addEventListener('click', startLogPolling);
    document.getElementById('logs-disconnect-btn').addEventListener('click', stopLogPolling);
    
    document.getElementById('logs-autoscroll').addEventListener('change', (e) => {
        logsAutoScroll = e.target.checked;
    });
}

// Initialize on DOM ready
document.addEventListener('DOMContentLoaded', initLogsViewer);
