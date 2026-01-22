"""
Convert RGB565 raw image files to PNG
Run on your Mac (not on the ESP32)

Usage: python convert_rgb565.py input.rgb565 [width] [height]

If width/height not specified, assumes VGA (640x480)
"""

import sys
from PIL import Image
import struct

def rgb565_to_rgb888(pixel):
    """Convert RGB565 (16-bit) to RGB888 (24-bit)"""
    r = (pixel >> 11) & 0x1F
    g = (pixel >> 5) & 0x3F
    b = pixel & 0x1F

    # Scale to 8-bit
    r = (r << 3) | (r >> 2)
    g = (g << 2) | (g >> 4)
    b = (b << 3) | (b >> 2)

    return (r, g, b)

def convert_rgb565_to_png(input_file, output_file=None, width=640, height=480):
    """Convert RGB565 raw file to PNG"""

    if output_file is None:
        output_file = input_file.replace('.rgb565', '.png')

    # Read raw data
    with open(input_file, 'rb') as f:
        data = f.read()

    expected_size = width * height * 2
    actual_size = len(data)

    print(f"Input file: {input_file}")
    print(f"File size: {actual_size} bytes")
    print(f"Expected for {width}x{height}: {expected_size} bytes")

    if actual_size != expected_size:
        # Try to guess dimensions
        pixels = actual_size // 2
        print(f"Size mismatch! File has {pixels} pixels")

        # Common resolutions to try
        resolutions = [
            (640, 480),   # VGA
            (320, 240),   # QVGA
            (160, 120),   # QQVGA
            (800, 600),   # SVGA
            (1024, 768),  # XGA
            (1280, 720),  # HD
            (1600, 1200), # UXGA
        ]

        for w, h in resolutions:
            if w * h == pixels:
                print(f"Auto-detected resolution: {w}x{h}")
                width, height = w, h
                break
        else:
            print("Could not auto-detect resolution. Trying anyway...")
            # Estimate square-ish dimensions
            import math
            side = int(math.sqrt(pixels))
            width = side
            height = pixels // side
            print(f"Guessing: {width}x{height}")

    # Create image
    img = Image.new('RGB', (width, height))
    pixels_data = []

    for i in range(0, min(len(data), width * height * 2), 2):
        # RGB565 is typically big-endian from ESP32 camera
        pixel = struct.unpack('>H', data[i:i+2])[0]
        pixels_data.append(rgb565_to_rgb888(pixel))

    img.putdata(pixels_data)
    img.save(output_file)
    print(f"Saved: {output_file}")

    return output_file

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python convert_rgb565.py input.rgb565 [width] [height]")
        print("Example: python convert_rgb565.py test_photo.rgb565 640 480")
        sys.exit(1)

    input_file = sys.argv[1]
    width = int(sys.argv[2]) if len(sys.argv) > 2 else 640
    height = int(sys.argv[3]) if len(sys.argv) > 3 else 480

    convert_rgb565_to_png(input_file, width=width, height=height)
