"""
RGB565 TCP Streaming Client for Xiao ESP32-S3 Sense

Usage: python streaming_client.py [host] [port]
"""

import socket
import struct
import numpy as np
import cv2
import sys
import time

DEFAULT_HOST = '192.168.1.6'
DEFAULT_PORT = 8080

def rgb565_to_bgr(data, width, height):
    """Convert RGB565 bytes to BGR image for OpenCV"""
    pixels = np.frombuffer(data, dtype='>u2')
    r = ((pixels >> 11) & 0x1F).astype(np.uint8)
    g = ((pixels >> 5) & 0x3F).astype(np.uint8)
    b = (pixels & 0x1F).astype(np.uint8)
    r = (r << 3) | (r >> 2)
    g = (g << 2) | (g >> 4)
    b = (b << 3) | (b >> 2)
    bgr = np.stack([b, g, r], axis=-1)
    return bgr.reshape(height, width, 3)

def recv_exact(sock, size):
    """Receive exactly 'size' bytes"""
    data = b''
    while len(data) < size:
        chunk = sock.recv(min(4096, size - len(data)))
        if not chunk:
            raise ConnectionError("Connection closed")
        data += chunk
    return data

def main():
    host = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_HOST
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT

    print(f"Connecting to {host}:{port} (TCP)...")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.connect((host, port))

    # Handshake
    sock.send(b'STREAM\n')

    # Get dimensions
    width = struct.unpack('>H', recv_exact(sock, 2))[0]
    height = struct.unpack('>H', recv_exact(sock, 2))[0]
    expected_size = width * height * 2

    print(f"Stream: {width}x{height} RGB565 ({expected_size} bytes/frame)")

    frame_count = 0
    last_time = time.time()

    try:
        while True:
            # Get frame length
            length = struct.unpack('>I', recv_exact(sock, 4))[0]

            # Get frame data
            frame_data = recv_exact(sock, length)

            # Convert and display
            frame = rgb565_to_bgr(frame_data, width, height)

            # Scale up for visibility if small
            if width < 320:
                frame = cv2.resize(frame, (width * 4, height * 4), interpolation=cv2.INTER_NEAREST)
            elif width < 640:
                frame = cv2.resize(frame, (width * 2, height * 2), interpolation=cv2.INTER_NEAREST)

            cv2.imshow('ESP32-S3 Camera', frame)
            frame_count += 1

            # FPS counter
            now = time.time()
            if now - last_time >= 1.0:
                fps = frame_count / (now - last_time)
                print(f"FPS: {fps:.1f}")
                frame_count = 0
                last_time = now

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    except KeyboardInterrupt:
        print("\nStopped")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
