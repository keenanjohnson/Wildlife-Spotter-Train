# Train Controller (Pybricks)

The code that runs via Pybricks firmware on the LEGO City Hub inside the train. The ESP32-S3 camera module connects to the hub via BLE and sends motor commands.

## Architecture

```
┌─────────────────┐     BLE GATT      ┌─────────────────┐
│   ESP32-S3      │ ──────────────────│  LEGO City Hub  │
│   (Camera)      │   stdin commands  │   (Pybricks)    │
│                 │                   │                 │
│  NimBLE Central │                   │  main.py        │
└─────────────────┘                   └─────────────────┘
```

## Programs

| File | Description |
|------|-------------|
| `main.py` | Production program - receives BLE stdin commands (F/B/S) and controls motor |
| `motor_test.py` | Standalone test - cycles motor forward/stop/backward without BLE |
| `check_ble.py` | Diagnostic - prints available Pybricks modules |

## Setup

```bash
# Create virtual environment (optional)
python -m venv env
source env/bin/activate

# Install pybricksdev
pip install pybricksdev

# Run program on hub (program stays installed after Ctrl+C)
pybricksdev run ble main.py
```

## Command Protocol

The ESP32 sends single-character commands via BLE stdin:

| Command | Action | Hub LED |
|---------|--------|---------|
| `F` | Forward (30% power) | Green |
| `B` | Backward (30% power) | Blue |
| `S` | Stop | Yellow |

The program responds via stdout (visible in ESP32 logs):

| Response | Meaning |
|----------|---------|
| `RDY` | Program ready, accepting commands |
| `FWD` | Forward command acknowledged |
| `BWD` | Backward command acknowledged |
| `STP` | Stop command acknowledged |

## Pybricks BLE Protocol

This section documents the Pybricks BLE protocol for connecting external devices (like ESP32) to a Pybricks hub.

### Service and Characteristic UUIDs

```
Pybricks Service:        c5f50001-8280-46da-89f4-6d8051e4aeef
Pybricks Characteristic: c5f50002-8280-46da-89f4-6d8051e4aeef
```

### Command Codes (Write to Characteristic)

| Code | Name | Description |
|------|------|-------------|
| `0x00` | STOP_USER_PROGRAM | Stop the running program |
| `0x01` | START_USER_PROGRAM | Start the installed user program |
| `0x02` | START_REPL | Start interactive REPL |
| `0x03` | WRITE_USER_PROGRAM_META | Write program metadata |
| `0x04` | WRITE_USER_RAM | Upload program binary |
| `0x05` | REBOOT_TO_UPDATE_MODE | Enter firmware update mode (purple LED) |
| `0x06` | WRITE_STDIN | Write data to program's stdin |
| `0x07` | WRITE_APP_DATA | Write to app data buffer (v1.4.0+) |

**IMPORTANT**: Command `0x05` triggers firmware update mode, NOT stdin! Use `0x06` for stdin.

### Event Codes (Notifications from Characteristic)

| Code | Name | Description |
|------|------|-------------|
| `0x00` | STATUS_REPORT | Hub status flags (sent every ~500ms) |
| `0x01` | STDOUT | Data from program's print() statements |

### Status Report Flags (Event 0x00, Byte 1)

| Bit | Meaning |
|-----|---------|
| 0x01 | Battery low |
| 0x02 | User program running |
| 0x40 | High current warning |

### Writing to stdin

To send data to a running program's stdin:

```
[0x06][data bytes...]
```

Example: Send "F" command
```
[0x06][0x46]  // 0x46 = ASCII 'F'
```

### Reading stdin in Pybricks

Use `stdin.buffer.read()` for raw bytes from BLE (not `stdin.read()`):

```python
from usys import stdin
from uselect import poll

keyboard = poll()
keyboard.register(stdin)

while True:
    if keyboard.poll(0):
        byte = stdin.buffer.read(1)  # Raw bytes from BLE
        if byte:
            cmd = chr(byte[0]).upper()
            # Process command...
```

**Key insight**: BLE stdin data arrives as raw bytes, so `stdin.buffer.read(1)` is required instead of `stdin.read(1)`.

## References

- [Pybricks BLE Profile](https://github.com/pybricks/technical-info/blob/master/pybricks-ble-profile.md)
- [Pybricks Hub-to-Device Communication](https://pybricks.com/projects/tutorials/wireless/hub-to-device/pc-communication/)
- [Brickrail BLE Implementation](https://github.com/Novakasa/brickrail/tree/master/ble-server)
