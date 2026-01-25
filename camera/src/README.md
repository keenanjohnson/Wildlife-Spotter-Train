

### Flashing Commands

In the terminal where you are going to use ESP-IDF, run:

```
~/esp/esp-idf/install.sh esp32s3
. ~/esp/esp-idf/export.sh"
idf.py fullclean
idf.py update-dependencies
rm -f sdkconfig sdkconfig.old
git submodule update --init --recursive --remote
idf.py set-target esp32s3

```