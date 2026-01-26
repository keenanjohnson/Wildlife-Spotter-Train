# ESP32-S3 Camera Firmware

MJPEG streaming server for the Wildlife Spotter Train camera module.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32-S3 (Dual Core)                    │
│                                                                 │
│  ┌─────────────────────────────┐  ┌───────────────────────────┐ │
│  │          CORE 0             │  │          CORE 1           │ │
│  │                             │  │                           │ │
│  │  ┌───────────────────────┐  │  │  ┌─────────────────────┐  │ │
│  │  │   API Server (:80)    │  │  │  │ Stream Server (:81) │  │ │
│  │  │   - Web UI (/)        │  │  │  │ - MJPEG stream      │  │ │
│  │  │   - Capture (/capture)│  │  │  │ - Continuous loop   │  │ │
│  │  │   - Status (/status)  │  │  │  │                     │  │ │
│  │  └───────────────────────┘  │  │  └──────────┬──────────┘  │ │
│  │                             │  │             │             │ │
│  │  ┌───────────────────────┐  │  │             │             │ │
│  │  │   WiFi + System       │  │  │  ┌──────────▼──────────┐  │ │
│  │  └───────────────────────┘  │  │  │   Camera Driver     │  │ │
│  │                             │  │  │   (JPEG capture)    │  │ │
│  └─────────────────────────────┘  │  └─────────────────────┘  │ │
│                                   └───────────────────────────┘ │
└───────────────────────────────────────────────────┬─────────────┘
                                                    │
                                                    ▼ WiFi
                                          ┌─────────────────┐
                                          │     Browser     │
                                          │    (Web UI)     │
                                          └─────────────────┘
```

### Dual-Core Design

The ESP32-S3 has two Xtensa LX7 cores. We split the workload to prevent the MJPEG streaming loop from blocking API requests:

| Core | Responsibility | Why |
|------|----------------|-----|
| **Core 0** | API server, WiFi, system tasks | Responsive to user interactions |
| **Core 1** | MJPEG streaming server | Continuous frame capture/send loop |

The MJPEG handler runs an infinite loop sending frames. Without core separation, this would block the single-threaded HTTP server from accepting new connections on other endpoints.

## HTTP Endpoints

The firmware runs two HTTP servers to prevent the streaming handler from blocking other requests:

| Endpoint | Port | Description |
|----------|------|-------------|
| `/` | 80 | Web UI with embedded video player and controls |
| `/capture` | 80 | Single JPEG snapshot |
| `/status` | 80 | JSON camera status and settings |
| `/stream` | 81 | MJPEG video stream |
| `/` | 81 | MJPEG video stream (alias) |

### Example Usage

```bash
# View web UI
open http://192.168.1.6/

# Capture single frame
curl http://192.168.1.6/capture -o snapshot.jpg

# Get camera status
curl http://192.168.1.6/status

# Direct stream access
open http://192.168.1.6:81/stream
```

## Source Files

| File | Description |
|------|-------------|
| `main/main.c` | Entry point, initialization sequence |
| `main/camera.h` | OV3660 sensor configuration, pin mappings |
| `main/wifi_sta.h` | WiFi station mode, auto-reconnect logic |
| `main/http_server.h` | Dual HTTP servers, MJPEG streaming, REST endpoints |
| `main/web_ui.h` | Embedded HTML/CSS/JS web interface |

## Camera Configuration

- **Sensor**: OV3660 (auto-detected, also supports OV2640)
- **Resolution**: HD 1280x720
- **Format**: JPEG (hardware encoding)
- **Quality**: 12/63 (lower = better quality)
- **Frame buffers**: 2 (continuous capture mode)
- **Frame buffer location**: PSRAM

## Build & Flash

### Prerequisites

```bash
# Source ESP-IDF environment
source ~/.espressif/v5.5.2/esp-idf/export.sh

# Set target (first time only)
idf.py set-target esp32s3
```

### WiFi Configuration

Before building, configure your WiFi credentials:

```bash
idf.py menuconfig
```

Navigate to **WiFi Configuration** and set your SSID and password. These settings are stored in `sdkconfig` which is gitignored.

### Build

```bash
idf.py build
```

### Flash & Monitor

```bash
idf.py -p /dev/cu.usbmodem* flash monitor
```

Press `Ctrl+]` to exit monitor.

## Performance

Typical performance on local WiFi:

| Metric | Value |
|--------|-------|
| Frame rate | 10-15 fps |
| Frame size | ~25 KB |
| Latency | 50-100 ms |
| Free heap | ~8 MB |

## Pin Configuration

```
OV3660 Camera Module:
  XCLK  = GPIO 10
  SIOD  = GPIO 40 (I2C SDA)
  SIOC  = GPIO 39 (I2C SCL)
  D7-D0 = GPIO 48,11,12,14,16,18,17,15
  VSYNC = GPIO 38
  HREF  = GPIO 47
  PCLK  = GPIO 13
  LED   = GPIO 21
```

## Credits

The initial ESP-IDF structure was heavily inspired by: https://github.com/wrsturgeon/esp32s3-video-stream
