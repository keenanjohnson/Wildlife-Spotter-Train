"""
Test script for SD card and camera on Xiao ESP32-S3
Takes a picture and saves it to the SD card
"""

import machine
import os

def init_sd():
    """Initialize SD card using machine.SDCard for Xiao ESP32-S3"""
    print("Initializing SD card...")

    sd = machine.SDCard(
        slot=3,
        width=1,
        sck=machine.Pin(7),
        mosi=machine.Pin(9),
        miso=machine.Pin(8),
        cs=machine.Pin(21)
    )

    print("SD card info:", sd.info())
    os.mount(sd, "/sd")

    print("SD card mounted at /sd")
    print("Files on SD card:", os.listdir("/sd"))
    return sd

def init_camera():
    """Initialize the camera in RGB565 mode"""
    print("Initializing camera...")
    from camera import Camera, PixelFormat, FrameSize

    cam = Camera()
    cam.init()

    print(f"Sensor: {cam.get_sensor_name()}")

    # Configure for RGB565 at VGA (640x480)
    cam.reconfigure(
        pixel_format=PixelFormat.RGB565,
        frame_size=FrameSize.VGA
    )

    print("Camera initialized (RGB565, VGA)")
    return cam

def take_photo(cam, filename="/sd/photo.rgb565"):
    """Capture a photo and save to SD card"""
    print("Capturing image...")

    img = cam.capture()

    if img is None:
        print("Error: Failed to capture image")
        return False

    print(f"Image captured, size: {len(img)} bytes")
    print(f"Saving to {filename}...")

    with open(filename, "wb") as f:
        f.write(img)

    print(f"Photo saved to {filename}")
    return True

def cleanup(cam):
    """Deinit camera and unmount SD card"""
    try:
        cam.deinit()
        print("Camera deinitialized")
    except:
        pass

    try:
        os.umount("/sd")
        print("SD card unmounted")
    except:
        pass

def main():
    """Main test function"""
    cam = None
    try:
        init_sd()
        cam = init_camera()
        take_photo(cam, "/sd/test_photo.rgb565")
        print(f"Files on SD card: {os.listdir('/sd')}")

    except Exception as e:
        import sys
        print(f"Error: {e}")
        sys.print_exception(e)
    finally:
        if cam:
            cleanup(cam)

if __name__ == "__main__":
    main()
