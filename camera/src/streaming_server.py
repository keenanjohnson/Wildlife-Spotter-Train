"""
RGB565 TCP Streaming Server for Xiao ESP32-S3 Sense

Streams raw RGB565 frames over TCP.
"""

import esp
import machine
import socket as soc
from time import sleep
import gc
from Wifi import Sta
from camera import Camera, PixelFormat, FrameSize, GrabMode

# Configuration
WIFI_SSID = 'Bill Wi the Science Fi'
WIFI_PASS = 'FIXME'
SERVER_PORT = 8080

esp.osdebug(None)

def init_camera():
    """Initialize camera in RGB565 mode with max performance"""
    # Set CPU to max frequency (240 MHz)
    machine.freq(240_000_000)
    print(f"CPU freq: {machine.freq() // 1_000_000} MHz")

    print("Initializing camera...")
    cam = Camera()
    cam.init()

    print(f"Sensor: {cam.get_sensor_name()}")

    # Reconfigure with performance settings
    cam.reconfigure(
        pixel_format=PixelFormat.RGB565,
        frame_size=FrameSize.QVGA,  # 320x240 - try larger now with optimizations
        grab_mode=GrabMode.LATEST   # Always get latest frame, skip old ones
    )

    width = cam.get_pixel_width()
    height = cam.get_pixel_height()

    print(f"Camera ready: RGB565 {width}x{height}")
    print(f"FB count: {cam.get_fb_count()}")
    return cam, width, height

def connect_wifi():
    """Connect to WiFi"""
    print(f"Connecting to WiFi: {WIFI_SSID}")

    sta = Sta()
    sta.wlan.disconnect()
    sta.connect(WIFI_SSID, WIFI_PASS)
    sta.wait()

    for _ in range(10):
        if sta.wlan.isconnected():
            ip = sta.wlan.ifconfig()[0]
            print(f"Connected! IP: {ip}")
            return ip
        print("Waiting for WiFi...")
        sleep(2)

    return None

def run_server(cam, width, height):
    """Run TCP streaming server"""
    addr = soc.getaddrinfo('0.0.0.0', SERVER_PORT)[0][-1]
    s = soc.socket(soc.AF_INET, soc.SOCK_STREAM)
    s.setsockopt(soc.SOL_SOCKET, soc.SO_REUSEADDR, 1)
    s.bind(addr)
    s.listen(1)

    frame_size = width * height * 2
    print(f"TCP Server on port {SERVER_PORT}")
    print(f"Frame: {width}x{height} = {frame_size} bytes")

    while True:
        print("Waiting for client...")
        cs, ca = s.accept()
        print(f"Client connected: {ca}")

        try:
            cs.setsockopt(soc.IPPROTO_TCP, soc.TCP_NODELAY, 1)

            # Wait for handshake
            data = cs.recv(64)
            if not data.startswith(b'STREAM'):
                cs.close()
                continue

            # Send dimensions
            cs.write(width.to_bytes(2, 'big'))
            cs.write(height.to_bytes(2, 'big'))

            print("Streaming...")
            frame_count = 0

            while True:
                img = cam.capture()
                if img is None:
                    continue

                # Send frame length + data
                cs.write(len(img).to_bytes(4, 'big'))
                cs.write(img)

                frame_count += 1
                if frame_count % 50 == 0:
                    gc.collect()
                if frame_count % 100 == 0:
                    print(f"Sent {frame_count} frames")

        except OSError as e:
            print(f"Error: {e}")
        finally:
            cs.close()
            print("Client disconnected")
            gc.collect()

def main():
    cam = None
    try:
        cam, width, height = init_camera()
        ip = connect_wifi()

        if not ip:
            print("WiFi connection failed!")
            return

        run_server(cam, width, height)

    except Exception as e:
        print(f"Error: {e}")
    finally:
        if cam:
            cam.deinit()

if __name__ == "__main__":
    main()
