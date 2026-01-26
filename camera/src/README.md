# ESP32-S3 Camera Firmware

MJPEG streaming server and BLE train controller for the Wildlife Spotter Train camera module.

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
│  │  │   - Train (/train)    │  │  │  └──────────┬──────────┘  │ │
│  │  └───────────────────────┘  │  │             │             │ │
│  │                             │  │             │             │ │
│  │  ┌───────────────────────┐  │  │  ┌──────────▼──────────┐  │ │
│  │  │   WiFi + NimBLE       │  │  │  │   Camera Driver     │  │ │
│  │  └───────────────────────┘  │  │  │   (JPEG capture)    │  │ │
│  │             │               │  │  └─────────────────────┘  │ │
│  └─────────────┼───────────────┘  └───────────────────────────┘ │
│                │                                                │
└────────────────┼────────────────────────────────────────────────┘
                 │ BLE                          │ WiFi
                 ▼                              ▼
       ┌─────────────────┐            ┌─────────────────┐
       │  LEGO City Hub  │            │     Browser     │
       │   (Pybricks)    │            │    (Web UI)     │
       └─────────────────┘            └─────────────────┘
```

### Dual-Core Design

The ESP32-S3 has two Xtensa LX7 cores. We split the workload to prevent the MJPEG streaming loop from blocking API requests:

| Core | Responsibility | Why |
|------|----------------|-----|
| **Core 0** | API server, WiFi, system tasks | Responsive to user interactions |
| **Core 1** | MJPEG streaming server | Continuous frame capture/send loop |

The MJPEG handler runs an infinite loop sending frames. Without core separation, this would block the single-threaded HTTP server from accepting new connections on other endpoints.

## mDNS / Bonjour

The camera advertises itself via mDNS, so you can access it without knowing the IP address:

| URL | Description |
|-----|-------------|
| `http://train.local/` | Web UI |
| `http://train.local/capture` | Single snapshot |
| `http://train.local/status` | Camera status JSON |
| `http://train.local:81/stream` | MJPEG stream |

mDNS works on macOS and Linux out of the box. On Windows, install [Bonjour Print Services](https://support.apple.com/kb/DL999).

## HTTP Endpoints

The firmware runs two HTTP servers to prevent the streaming handler from blocking other requests:

| Endpoint | Port | Description |
|----------|------|-------------|
| `/` | 80 | Web UI with embedded video player and train controls |
| `/capture` | 80 | Single JPEG snapshot |
| `/status` | 80 | JSON camera status and settings |
| `/train` | 80 | Train control API (see below) |
| `/stream` | 81 | MJPEG video stream |
| `/` | 81 | MJPEG video stream (alias) |

### Train Control API

Control the LEGO train via the `/train` endpoint:

```bash
# Move forward
curl "http://train.local/train?action=forward"

# Move backward
curl "http://train.local/train?action=backward"

# Stop
curl "http://train.local/train?action=stop"

# Check status
curl "http://train.local/train?action=status"
```

Response format:
```json
{"status": "ok", "action": "forward", "train_state": "ready"}
```

### Example Usage

```bash
# View web UI (using mDNS hostname)
open http://train.local/

# Or using IP address directly
open http://192.168.1.6/

# Capture single frame
curl http://train.local/capture -o snapshot.jpg

# Get camera status
curl http://train.local/status

# Direct stream access
open http://train.local:81/stream
```

## Source Files

| File | Description |
|------|-------------|
| `main/main.c` | Entry point, initialization sequence |
| `main/camera.h` | OV3660 sensor configuration, pin mappings |
| `main/wifi_sta.h` | WiFi station mode, auto-reconnect logic |
| `main/mdns_service.h` | mDNS/Bonjour hostname advertisement |
| `main/http_server.h` | Dual HTTP servers, MJPEG streaming, REST endpoints |
| `main/web_ui.h` | Embedded HTML/CSS/JS web interface |
| `main/train_ble.h` | NimBLE GATT client for Pybricks hub communication |

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

## BLE Train Control

The ESP32-S3 acts as a BLE Central (GATT client) to control the LEGO City Hub running Pybricks firmware. WiFi and BLE run concurrently using the ESP32's coexistence feature.

### How It Works

1. **Scan**: ESP32 scans for devices advertising the Pybricks Service UUID
2. **Connect**: Connects to the hub and discovers the Pybricks characteristic
3. **Subscribe**: Enables notifications to receive stdout from the hub
4. **Start Program**: Sends command `0x01` to start the pre-installed user program
5. **Wait for Ready**: Waits for the program to print "RDY" via stdout
6. **Send Commands**: Writes stdin commands (`0x06` + data) to control the motor

### Configuration (sdkconfig.defaults)

```ini
# Bluetooth / NimBLE
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_CENTRAL=y
CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=158

# WiFi/BLE Coexistence
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y
CONFIG_ESP_COEX_POWER_MANAGEMENT=y
```

### Connection States

| State | Description |
|-------|-------------|
| `disconnected` | Not connected, will scan |
| `scanning` | Scanning for Pybricks hub |
| `connecting` | Found hub, connecting |
| `discovering` | Connected, discovering services |
| `initializing` | Starting user program |
| `ready` | Program running, accepting commands |

### Prerequisites

Before the ESP32 can control the train:

1. Flash Pybricks firmware to the LEGO City Hub
2. Install the train program: `pybricksdev run ble train/main.py`
3. The program stays installed after Ctrl+C

See [train/README.md](../../../train/README.md) for the Pybricks BLE protocol details.

## Credits

The initial ESP-IDF structure was heavily inspired by: https://github.com/wrsturgeon/esp32s3-video-stream
