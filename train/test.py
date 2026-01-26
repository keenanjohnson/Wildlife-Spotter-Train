from pybricks.hubs import CityHub
from pybricks.parameters import Color, Port
from pybricks.tools import wait
from pybricks.pupdevices import DCMotor
from bleradio import BLERadio

# Initialize the hub and motor
hub = CityHub()
train_motor = DCMotor(Port.A)

# BLE Radio setup:
# - Broadcast status on channel 1
# - Observe commands from ESP32 on channel 2
radio = BLERadio(broadcast_channel=1, observe_channels=[2])

# Signal ready
hub.light.on(Color.GREEN)
print("RDY")

# Current state
current_speed = 0

while True:
    # Check for incoming commands from ESP32
    data = radio.observe(2)

    if data is not None and len(data) > 0:
        cmd = data[0] if isinstance(data, (list, tuple)) else data

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

    # Broadcast current status (so ESP32 knows we're alive)
    radio.broadcast(["OK", current_speed])

    # Small delay
    wait(100)
