## Set up esp project

```
source ~/.espressif/v5.5.2/esp-idf/export.sh
idf.py set-target esp32s3
```

## WiFi Configuration

Before building, configure your WiFi credentials:

```
source ~/.espressif/v5.5.2/esp-idf/export.sh
idf.py menuconfig
```

Navigate to **WiFi Configuration** and set your SSID and password. These settings are stored in `sdkconfig` which is gitignored.

## Commands to build

```
source ~/.espressif/v5.5.2/esp-idf/export.sh
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
