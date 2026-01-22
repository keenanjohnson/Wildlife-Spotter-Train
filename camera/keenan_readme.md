
# Camera API Firmware

https://github.com/cnadler86/micropython-camera-API


## Erase flash

```
esptool --port /dev/tty.usbmodem101 erase_flash
```

# Testing mp cam

from https://github.com/cnadler86/micropython-camera-API

### Bringup commands

```
from camera import Camera, GrabMode, PixelFormat, FrameSize, GainCeiling

cam = Camera()

cam.init()

img = cam.capture()
```


## SD Card Pins

ESP32-S3 GPIO	microSD Card Slot
GPIO21	CS
D8 / A8 / Qt7 / GPIO7	SCK
D9 / A9 / Qt8 / GPIO8	MISO
D10 / A10 / Qt9 / GPIO9	MOSI

SD Card from docs

```
import machine, os, vfs

sd = machine.SDCard(slot=2, sck=7, cs=3, miso=8, mosi=9)
vfs.mount(sd, '/sd') # mount

os.listdir('/sd')    # list directory contents
vfs.umount('/sd')    # eject
```

```
spi = machine.SPI( 2, baudrate=4000000, polarity=0, phase=0, sck=machine.Pin(7),mosi=machine.Pin(9), miso=machine.Pin(8))

cs = machine.Pin(21)
```

