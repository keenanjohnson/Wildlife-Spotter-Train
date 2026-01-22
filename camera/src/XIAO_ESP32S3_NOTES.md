# Xiao ESP32-S3 Sense - MicroPython Board Bring-Up Notes

## Hardware
- **Board:** Seeed Studio XIAO ESP32S3 Sense
- **Camera Sensor:** OV3660 (3MP)
- **MicroPython:** v1.27.0-dirty on 2026-01-12; Generic ESP32S3 module with Octal-SPIRAM

---

## SD Card

### Pin Mapping
| SD Card Function | ESP32-S3 GPIO |
|------------------|---------------|
| CS               | GPIO21        |
| SCK              | GPIO7         |
| MISO             | GPIO8         |
| MOSI             | GPIO9         |

### Working Configuration
Use `machine.SDCard` directly (not SPI + sdcard module):

```python
import machine
import os

sd = machine.SDCard(
    slot=3,
    width=1,
    sck=machine.Pin(7),
    mosi=machine.Pin(9),
    miso=machine.Pin(8),
    cs=machine.Pin(21)
)

os.mount(sd, "/sd")
print(os.listdir("/sd"))
os.umount("/sd")
```

### Alternative: SPI + sdcard module
This also works but requires the `sdcard.py` driver and a slower baudrate:

```python
import machine
import os
import sdcard

spi = machine.SPI(1, baudrate=400000, polarity=0, phase=0,
                  sck=machine.Pin(7),
                  mosi=machine.Pin(9),
                  miso=machine.Pin(8))
cs = machine.Pin(21, machine.Pin.OUT)

# Reset sequence (helps with reliability)
cs(1)
spi.write(bytearray([0xFF] * 16))
cs(0)
spi.write(bytearray([0xFF] * 4))
cs(1)

sd = sdcard.SDCard(spi, cs)
vfs = os.VfsFat(sd)
os.mount(vfs, "/sd")
```

### Troubleshooting SD Card
- **OSError: timeout waiting for v2 card** - Use slower baudrate (400000 or lower), check wiring
- **OSError: [Errno 19] ENODEV** - Card not detected, check insertion/wiring
- The `sdcard.py` driver needs to be uploaded to the board if using SPI method

---

## Camera

### Sensor Info
- **Sensor:** OV3660
- **Max frame size:** 19 (QXGA 2048x1536)

### What Works
- **RGB565 format** - Works reliably
- **VGA resolution (640x480)** - 614,400 bytes per frame
- **QVGA resolution (320x240)** - 153,600 bytes per frame

### What Does NOT Work
- **JPEG format** - `capture()` returns `None` when pixel_format is set to JPEG
  - This appears to be a limitation of this MicroPython camera driver with the OV3660 sensor
  - The pixel format can be set to JPEG (value 4), but capture fails

### Working Camera Configuration

```python
from camera import Camera, PixelFormat, FrameSize

cam = Camera()
cam.init()

# Configure for RGB565 at VGA
cam.reconfigure(
    pixel_format=PixelFormat.RGB565,
    frame_size=FrameSize.VGA
)

# Capture
img = cam.capture()  # Returns bytes, 614400 for VGA RGB565

# Cleanup
cam.deinit()
```

### Camera API Notes
- `cam.init()` does NOT accept keyword arguments
- Use `cam.reconfigure()` to change pixel format and frame size after init
- `pixel_format` property is read-only (no setter)
- Available GrabModes: `LATEST`, `WHEN_EMPTY`
- PixelFormat values: `GRAYSCALE`, `JPEG` (4), `RAW`, `RGB444`, `RGB555`, `RGB565` (0), `RGB888`, `YUV420`, `YUV422`

### Available Camera Methods
```
capture, deinit, init, reconfigure,
get_pixel_format, get_frame_size, get_quality, get_sensor_name,
get_brightness, get_contrast, get_saturation, get_sharpness,
get_exposure_ctrl, get_gain_ctrl, get_whitebal, get_hmirror, get_vflip,
set_quality, set_brightness, set_contrast, set_saturation, set_sharpness,
set_exposure_ctrl, set_gain_ctrl, set_whitebal, set_hmirror, set_vflip,
frame_available, get_fb_count, free_buffer
```

---

## Converting RGB565 Images

RGB565 raw files need to be converted to view on a computer. Use `convert_rgb565.py`:

```bash
pip install Pillow
python convert_rgb565.py test_photo.rgb565 640 480
```

This creates a PNG file that can be viewed normally.

### RGB565 Format Details
- 16 bits per pixel (2 bytes)
- Red: 5 bits, Green: 6 bits, Blue: 5 bits
- VGA (640x480): 614,400 bytes
- QVGA (320x240): 153,600 bytes
- Big-endian byte order from ESP32 camera

---

## mpremote Commands

```bash
# List devices
mpremote devs

# Connect to specific device
mpremote connect /dev/cu.usbmodem101

# Reset board
mpremote connect /dev/cu.usbmodem101 reset

# Copy file to board
mpremote connect /dev/cu.usbmodem101 cp local_file.py :remote_file.py

# Run script
mpremote connect /dev/cu.usbmodem101 run script.py

# Mount local directory (files appear under /remote/)
mpremote connect /dev/cu.usbmodem101 mount .
```

---

## Known Issues

1. **JPEG capture not working** - The MicroPython camera driver doesn't properly support JPEG encoding for OV3660 on this build. Use RGB565 and convert on computer.

2. **Module-level camera API missing** - `camera.init()` as a module function doesn't exist. Must use `Camera()` class.

3. **SD card init can be flaky** - Sometimes needs reset sequence or slower baudrate for reliable initialization.
