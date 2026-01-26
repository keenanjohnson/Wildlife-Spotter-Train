# Check what's available in pybricks.experimental
from pybricks.hubs import CityHub
from pybricks.parameters import Color

hub = CityHub()
hub.light.on(Color.GREEN)

import pybricks.experimental as exp
print("experimental contents:")
print(dir(exp))

try:
    import pybricks.iodevices as io
    print("iodevices contents:")
    print(dir(io))
except Exception as e:
    print("iodevices error:", e)

print("Done")
