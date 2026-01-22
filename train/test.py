from pybricks.hubs import CityHub
from pybricks.parameters import Color, Port
from pybricks.tools import wait
from pybricks.pupdevices import DCMotor

# Initialize the hub
hub = CityHub()

# Initialize the motor.
train_motor = DCMotor(Port.A)

# Keep blinking Blue
hub.light.blink(Color.GREEN, [500, 500])

# Choose the "power" level for your train. Negative means reverse.
train_motor.dc(30)

# Keep doing nothing. The train just keeps going.
while True:
    wait(1000)