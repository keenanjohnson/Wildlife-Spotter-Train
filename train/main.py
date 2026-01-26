# Train control via stdin commands
# Install this program to the hub using Pybricks app, then ESP32 can connect via GATT

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

# Current state
current_speed = 0

while True:
    # Check for incoming commands via stdin (from BLE GATT)
    while keyboard.poll(0):
        try:
            cmd = stdin.readline().strip().upper()
            if cmd == "F":
                train_motor.dc(30)
                hub.light.on(Color.GREEN)
                current_speed = 30
                print("FWD")
            elif cmd == "B":
                train_motor.dc(-30)
                hub.light.on(Color.BLUE)
                current_speed = -30
                print("BWD")
            elif cmd == "S":
                train_motor.dc(0)
                hub.light.on(Color.YELLOW)
                current_speed = 0
                print("STP")
        except:
            pass

    wait(50)
