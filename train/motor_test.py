# Simple motor test - cycles through forward/stop/backward
from pybricks.hubs import CityHub
from pybricks.parameters import Color, Port
from pybricks.tools import wait
from pybricks.pupdevices import DCMotor

hub = CityHub()
train_motor = DCMotor(Port.A)

hub.light.on(Color.GREEN)
print("Motor test starting...")

# Forward
print("Forward")
hub.light.on(Color.GREEN)
train_motor.dc(30)
wait(2000)

# Stop
print("Stop")
hub.light.on(Color.YELLOW)
train_motor.dc(0)
wait(1000)

# Backward
print("Backward")
hub.light.on(Color.BLUE)
train_motor.dc(-30)
wait(2000)

# Stop
print("Stop")
hub.light.on(Color.YELLOW)
train_motor.dc(0)

print("Test complete!")
