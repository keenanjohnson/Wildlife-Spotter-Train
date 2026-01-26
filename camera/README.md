# Camera Module

The camera module captures video from the train and streams it wirelessly to a desktop receiver. It uses a XIAO ESP32-S3 Sense board with an OV2640 camera sensor, housed in a custom 3D-printed enclosure.

## Features

- HD 1280x720 JPEG video streaming
- UDP-based transmission with chunked frames for reliability
- WiFi station mode with auto-reconnect
- Adaptive frame rate (33-1000ms based on conditions)

## Subfolders

- **[cad/](cad/)** - 3D-printable enclosure design (Onshape source + STL files)
- **[src/](src/)** - ESP-IDF C firmware for the ESP32-S3

