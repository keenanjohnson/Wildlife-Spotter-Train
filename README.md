# Wildlife Spotter Train

Explore the wilderness via Train! BLE-controlled LEGO City train using an ESP32S3 microcontroller. Features real-time video streaming and playful interaction design. Currently in active development.

This project is a Work in Progress but features:

- A small camera module attached to an esp32-s3 microcontroller
- A 3d printed enclosure with ingrated stand and antennae placement as well as lego xonpatible mounts 
- Micropython software running on the train to receive drive conmands via bluetooth 
- Software on thr microcontroller in C to send commands to the teain via bluetooth, receive user commands from a webpage or app, and stream video to the web pagebor app. 
- Fun

## The story

I recently found myself in possesion of one of the Lego artic train sets, which is among one of the most fun recent kits. [kit link #60470](https://www.lego.com/en-us/product/explorers-arctic-polar-express-train-60470). On the back of the artic train is a camera tripod for spotting wildlife, specifically the artic foxes that are included in the kit.

<img width="1600" height="900" alt="image" src="https://github.com/user-attachments/assets/756d11d1-9e25-43b0-8ed7-c3e32e00f99c" />

I have recently been doing much ecology and as an embedded systems engineer, I thought it would be fun to improve the set by adding a real camera to the kit.

Additionaly, I had seen the excelent pybricks project and realized that I could also via Bluetooth control make a device and software gui that would not only stream video but also control the driving of the train. 
