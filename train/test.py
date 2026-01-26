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
    # Check for incoming commands (non-blocking)
    while keyboard.poll(0):
        cmd = stdin.readline().strip().upper()
        if cmd == "F":
            train_motor.dc(30)
            hub.light.on(Color.GREEN)
        elif cmd == "B":
            train_motor.dc(-30)
            hub.light.on(Color.BLUE)
        elif cmd == "S":
            train_motor.dc(0)
            hub.light.on(Color.YELLOW)

    # Small delay to prevent busy-waiting
    wait(50)
