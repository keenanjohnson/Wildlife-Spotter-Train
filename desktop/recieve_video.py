#!/usr/bin/env python3

import cv2
import numpy as np
import socket
import struct
import sys

PORT = 5005
HEADER_FORMAT = '<HHH'
CHUNK_SIZE = 1400
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# Large receive buffer to avoid OS-level packet drops
RECV_BUF_SIZE = 4 * 1024 * 1024  # 4 MB

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, RECV_BUF_SIZE)
sock.bind(('', PORT))
sock.settimeout(2.0)

print(f"Listening for frames on UDP port {PORT}...")

def recv_packet():
    """Blocking receive with timeout."""
    try:
        return sock.recv(CHUNK_SIZE + HEADER_SIZE + 64)
    except socket.timeout:
        return None

def drain_to_latest():
    """Drain socket buffer and return the most recent packet (non-blocking)."""
    sock.setblocking(False)
    latest = None
    try:
        while True:
            latest = sock.recv(CHUNK_SIZE + HEADER_SIZE + 64)
    except BlockingIOError:
        pass
    sock.settimeout(2.0)
    return latest

while True:
    # Phase 1: Find the start of a frame (chunk_id == 0)
    # Drain stale packets and grab the latest one
    packet = drain_to_latest()
    if packet is None:
        packet = recv_packet()
    if packet is None:
        continue

    while True:
        frame_id, chunk_id, total_chunks = struct.unpack(HEADER_FORMAT, packet[:HEADER_SIZE])
        if chunk_id == 0:
            break
        packet = recv_packet()
        if packet is None:
            break
    if packet is None:
        continue

    # Phase 2: Assemble the frame sequentially — don't skip any chunks
    jpeg_parts = []
    success = True

    for i in range(total_chunks):
        data = packet[HEADER_SIZE:]
        data_len = len(data)

        if i < total_chunks - 1 and data_len != CHUNK_SIZE:
            success = False
            break

        jpeg_parts.append(data)

        if i < total_chunks - 1:
            packet = recv_packet()
            if packet is None:
                success = False
                break
            fid, cid, tc = struct.unpack(HEADER_FORMAT, packet[:HEADER_SIZE])
            if fid != frame_id or cid != i + 1:
                # Frame got interrupted — abandon and start over
                success = False
                break

    if not success:
        continue

    # Phase 3: Decode and display
    jpeg_data = b''.join(jpeg_parts)
    arr = np.frombuffer(jpeg_data, dtype=np.uint8)
    im = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    if im is not None:
        cv2.imshow('ESP32-S3 Stream', im)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            cv2.destroyAllWindows()
            sys.exit(0)
