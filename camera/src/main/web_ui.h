#pragma once

// Embedded web UI for Wildlife Spotter Train
// Displays MJPEG stream and (future) train controls

static const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Wildlife Spotter Train</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
        }
        h1 {
            margin-bottom: 20px;
            font-weight: 300;
            font-size: 1.5rem;
        }
        .main-content {
            display: flex;
            gap: 20px;
            align-items: flex-start;
            flex-wrap: wrap;
            justify-content: center;
        }
        .video-section {
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        .video-container {
            background: #000;
            border-radius: 8px;
            overflow: hidden;
            box-shadow: 0 4px 20px rgba(0,0,0,0.5);
            max-width: 100%;
        }
        #stream {
            display: block;
            max-width: 100%;
            height: auto;
        }
        .status {
            margin-top: 15px;
            padding: 10px 20px;
            background: #16213e;
            border-radius: 4px;
            font-size: 0.9rem;
            color: #888;
        }
        .status.connected {
            color: #4ade80;
        }
        .status.error {
            color: #f87171;
        }
        .controls {
            margin-top: 15px;
            display: flex;
            gap: 10px;
        }
        button {
            padding: 12px 24px;
            font-size: 1rem;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: all 0.2s;
        }
        .btn-primary {
            background: #3b82f6;
            color: white;
        }
        .btn-primary:hover {
            background: #2563eb;
        }
        .btn-secondary {
            background: #374151;
            color: white;
        }
        .btn-secondary:hover {
            background: #4b5563;
        }
        .stats {
            margin-top: 10px;
            font-size: 0.8rem;
            color: #666;
        }
        .battery-indicator {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 8px 16px;
            background: #16213e;
            border-radius: 4px;
            margin-bottom: 20px;
        }
        .battery-icon {
            width: 32px;
            height: 16px;
            border: 2px solid #888;
            border-radius: 3px;
            position: relative;
            display: flex;
            align-items: center;
            padding: 2px;
        }
        .battery-icon::after {
            content: '';
            position: absolute;
            right: -5px;
            top: 50%;
            transform: translateY(-50%);
            width: 3px;
            height: 8px;
            background: #888;
            border-radius: 0 2px 2px 0;
        }
        .battery-level {
            height: 100%;
            background: #4ade80;
            border-radius: 1px;
            transition: width 0.3s, background 0.3s;
        }
        .battery-level.low {
            background: #f87171;
        }
        .battery-level.medium {
            background: #fbbf24;
        }
        .battery-text {
            font-size: 0.85rem;
            color: #aaa;
        }
        .battery-voltage {
            font-size: 0.75rem;
            color: #666;
        }
        .train-controls {
            padding: 20px;
            background: #16213e;
            border-radius: 8px;
            text-align: center;
            min-width: 180px;
        }
        .train-controls h2 {
            font-size: 1.1rem;
            font-weight: 300;
            margin-bottom: 15px;
        }
        .train-status {
            padding: 8px 16px;
            margin-bottom: 15px;
            border-radius: 4px;
            background: #374151;
            font-size: 0.85rem;
        }
        .train-status.connected { background: #166534; }
        .train-status.scanning { background: #854d0e; }
        .train-status.error { background: #991b1b; }
        .train-buttons {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .btn-train {
            padding: 20px 24px;
            font-size: 1.1rem;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.15s;
            user-select: none;
            -webkit-user-select: none;
            touch-action: manipulation;
        }
        .btn-forward { background: #22c55e; color: white; }
        .btn-forward:hover { background: #16a34a; }
        .btn-forward:active { background: #15803d; transform: scale(0.95); }
        .btn-stop { background: #ef4444; color: white; }
        .btn-stop:hover { background: #dc2626; }
        .btn-stop:active { background: #b91c1c; transform: scale(0.95); }
        .btn-backward { background: #3b82f6; color: white; }
        .btn-backward:hover { background: #2563eb; }
        .btn-backward:active { background: #1d4ed8; transform: scale(0.95); }
        @media (max-width: 700px) {
            .main-content {
                flex-direction: column;
            }
            .train-buttons {
                flex-direction: row;
            }
        }
    </style>
</head>
<body>
    <h1>Wildlife Spotter Train</h1>

    <div class="battery-indicator">
        <div class="battery-icon">
            <div id="battery-level" class="battery-level" style="width: 0%"></div>
        </div>
        <span id="battery-text" class="battery-text">---%</span>
        <span id="battery-voltage" class="battery-voltage">(-.--V)</span>
    </div>

    <div class="main-content">
        <div class="video-section">
            <div class="video-container">
                <img id="stream" src="" alt="Camera Stream">
            </div>

            <div id="status" class="status">Connecting...</div>

            <div class="controls">
                <button class="btn-primary" onclick="startStream()">Start Stream</button>
                <button class="btn-secondary" onclick="captureImage()">Capture</button>
            </div>

            <div class="stats" id="stats"></div>
        </div>

        <div class="train-controls">
            <h2>Train</h2>
            <div id="train-status" class="train-status">Checking...</div>
            <div class="train-buttons">
                <button class="btn-train btn-forward"
                        onmousedown="trainControl('forward')"
                        onmouseup="trainControl('stop')"
                        onmouseleave="trainControl('stop')"
                        ontouchstart="trainControl('forward'); event.preventDefault();"
                        ontouchend="trainControl('stop')">
                    &#9650; Fwd
                </button>
                <button class="btn-train btn-stop" onclick="trainControl('stop')">
                    Stop
                </button>
                <button class="btn-train btn-backward"
                        onmousedown="trainControl('backward')"
                        onmouseup="trainControl('stop')"
                        onmouseleave="trainControl('stop')"
                        ontouchstart="trainControl('backward'); event.preventDefault();"
                        ontouchend="trainControl('stop')">
                    &#9660; Back
                </button>
            </div>
        </div>
    </div>

    <script>
        const streamImg = document.getElementById('stream');
        const statusDiv = document.getElementById('status');
        const statsDiv = document.getElementById('stats');

        let frameCount = 0;
        let lastTime = Date.now();

        function startStream() {
            // Stream is on port 81 (separate server)
            const streamUrl = 'http://' + window.location.hostname + ':81/stream?' + Date.now();
            streamImg.src = streamUrl;
            statusDiv.textContent = 'Streaming...';
            statusDiv.className = 'status connected';
        }

        function captureImage() {
            // Open capture in new tab
            window.open('/capture?' + Date.now(), '_blank');
        }

        streamImg.onload = function() {
            frameCount++;
            const now = Date.now();
            if (now - lastTime >= 2000) {
                const fps = (frameCount / ((now - lastTime) / 1000)).toFixed(1);
                statsDiv.textContent = 'Client FPS: ' + fps;
                frameCount = 0;
                lastTime = now;
            }
        };

        streamImg.onerror = function() {
            statusDiv.textContent = 'Stream error - click Start to retry';
            statusDiv.className = 'status error';
        };

        // Battery monitoring
        const batteryLevel = document.getElementById('battery-level');
        const batteryText = document.getElementById('battery-text');
        const batteryVoltage = document.getElementById('battery-voltage');

        async function updateBattery() {
            try {
                const response = await fetch('/battery');
                const data = await response.json();

                batteryLevel.style.width = data.percentage + '%';
                batteryText.textContent = data.percentage + '%';
                batteryVoltage.textContent = '(' + data.voltage.toFixed(2) + 'V)';

                // Update color based on level
                batteryLevel.classList.remove('low', 'medium');
                if (data.percentage <= 20) {
                    batteryLevel.classList.add('low');
                } else if (data.percentage <= 50) {
                    batteryLevel.classList.add('medium');
                }
            } catch (err) {
                batteryText.textContent = 'N/A';
                batteryVoltage.textContent = '';
            }
        }

        // Auto-start stream on page load
        window.onload = function() {
            startStream();
            updateTrainStatus();
            updateBattery();
        };

        // Train control
        const trainStatusDiv = document.getElementById('train-status');
        let lastAction = null;

        async function trainControl(action) {
            // Debounce repeated stop commands
            if (action === lastAction) return;
            lastAction = action;

            try {
                const response = await fetch('/train?action=' + action);
                const data = await response.json();
                updateTrainStatusUI(data.state, data.result);
            } catch (err) {
                trainStatusDiv.textContent = 'Connection error';
                trainStatusDiv.className = 'train-status error';
            }

            // Reset lastAction for stop to allow repeated stops
            if (action === 'stop') {
                setTimeout(() => { lastAction = null; }, 100);
            }
        }

        function updateTrainStatusUI(state, result) {
            let statusText = state;
            let statusClass = 'train-status';

            if (state === 'ready') {
                statusText = 'Connected - ' + (result || 'Ready');
                statusClass += ' connected';
            } else if (state === 'scanning' || state === 'connecting' || state === 'discovering') {
                statusText = 'Connecting to train...';
                statusClass += ' scanning';
            } else if (state === 'disconnected') {
                statusText = 'Train disconnected - scanning...';
                statusClass += ' error';
            }

            trainStatusDiv.textContent = statusText;
            trainStatusDiv.className = statusClass;
        }

        async function updateTrainStatus() {
            try {
                const response = await fetch('/train?action=status');
                const data = await response.json();
                updateTrainStatusUI(data.state, null);
            } catch (err) {
                trainStatusDiv.textContent = 'Connection error';
                trainStatusDiv.className = 'train-status error';
            }
        }

        // Poll train status every 3 seconds
        setInterval(updateTrainStatus, 3000);

        // Poll battery every 10 seconds
        setInterval(updateBattery, 10000);
    </script>
</body>
</html>
)rawliteral";

// Handler for serving the index page
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}
