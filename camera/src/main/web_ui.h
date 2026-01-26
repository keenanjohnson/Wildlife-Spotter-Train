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
            margin-top: 20px;
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
            margin-top: 20px;
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
            margin-top: 20px;
            font-size: 0.8rem;
            color: #666;
        }
    </style>
</head>
<body>
    <h1>Wildlife Spotter Train</h1>

    <div class="video-container">
        <img id="stream" src="" alt="Camera Stream">
    </div>

    <div id="status" class="status">Connecting...</div>

    <div class="controls">
        <button class="btn-primary" onclick="startStream()">Start Stream</button>
        <button class="btn-secondary" onclick="captureImage()">Capture</button>
    </div>

    <div class="stats" id="stats"></div>

    <script>
        const streamImg = document.getElementById('stream');
        const statusDiv = document.getElementById('status');
        const statsDiv = document.getElementById('stats');

        let frameCount = 0;
        let lastTime = Date.now();

        function startStream() {
            // Use the current host for the stream URL
            const streamUrl = '/stream?' + Date.now();
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

        // Auto-start stream on page load
        window.onload = function() {
            startStream();
        };
    </script>
</body>
</html>
)rawliteral";

// Handler for serving the index page
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}
