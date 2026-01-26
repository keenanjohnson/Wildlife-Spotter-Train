# Train control via stdin commands (single character)
# Install this program to the hub, then ESP32 can connect via GATT

from pybricks.hubs import CityHub
from pybricks.parameters import Color, Port
from pybricks.tools import wait
from pybricks.pupdevices import DCMotor
from usys import stdin
from uselect import poll

# Initialize the hub and motor
hub = CityHub()
train_motor = DCMotor(Port.A)

# Set up stdin polling for non-blocking reads
keyboard = poll()
keyboard.register(stdin)

# Signal ready
hub.light.on(Color.GREEN)
print("RDY")

while True:
    # Check for incoming commands via stdin (from BLE GATT)
    # Use stdin.buffer.read() for raw bytes from BLE
    while keyboard.poll(0):
        byte = stdin.buffer.read(1)
        if byte:
            cmd = chr(byte[0]).upper()
            if cmd == "F":
                train_motor.dc(30)
                hub.light.on(Color.GREEN)
                print("FWD")
            elif cmd == "B":
                train_motor.dc(-30)
                hub.light.on(Color.BLUE)
                print("BWD")
            elif cmd == "S":
                train_motor.dc(0)
                hub.light.on(Color.YELLOW)
                print("STP")
            # Ignore other characters

    wait(50)
