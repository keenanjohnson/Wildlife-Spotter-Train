
## Commands to build

```
source ~/.espressif/v5.5.2/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build

```

## Commands to flash

```
idf.py -p /dev/cu.usbmodem* flash
```

## Monitor

```
idf.py -p /dev/cu.usbmodem* monitor
```

## Flash and Monitor

```
idf.py -p /dev/cu.usbmodem* flash monitor
```


## Credits

The initial esp-idf structure was heavily inspired by the work here: https://github.com/wrsturgeon/esp32s3-video-stream/tree/main
