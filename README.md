# Wildlife Spotter Train

Explore the wilderness via Train! BLE-controlled LEGO City train using an ESP32S3 microcontroller. Features real-time video streaming and playful interaction design. Currently in active development.

This project is a Work in Progress but features:

- A small camera module attached to an esp32-s3 microcontroller
- A 3d printed enclosure with ingrated stand and antennae placement as well as lego xonpatible mounts 
- Micropython software running on the train to receive drive conmands via bluetooth 
- Software on thr microcontroller in C to send commands to the teain via bluetooth, receive user commands from a webpage or app, and stream video to the web pagebor app. 
- Fun

## The story

    I gound myself in possesion of one of the Lego artic train sets, which is among one of the most fun recent kits. kit: https://www.lego.com/en-us/product/explorers-arctic-polar-express-train-60470?ef_id=Cj0KCQiA1czLBhDhARIsAIEc7uguyI1XteHayy08KlyCDtzCPm_7fJdRzck9Qq0AXHX6i1mzMNYerd0aAoGUEALw_wcB:G:s&s_kwcid=AL!790!3!753174751885!e!!g!!lego%20arctic%20polar%20express%20train!20573417184!180355510712&cmp=KAC-INI-GOOGUS-GO-US_GL-EN-RE-PS-BUY-EXPLORE-CITY_OTHERS-SHOP-BP-MM-ALL-CIDNA00000-NOVELTIES-CITY_OTHERS&gad_source=1&gad_campaignid=20573417184&gbraid=0AAAAADESMXIBHkpe-ZVZ_urViNnalKMUD&gclid=Cj0KCQiA1czLBhDhARIsAIEc7uguyI1XteHayy08KlyCDtzCPm_7fJdRzck9Qq0AXHX6i1mzMNYerd0aAoGUEALw_wcB

on the back of the artic train is a camera tripod for spotting wildlife, specifically the artic foxes that are included in the kit. 

I have recently been doing much ecology and as an embedded systems engineer, I thought it would be fun to imrpvoe the set by adding a real camera to the kit.

Additionaly, i had seen the excelent pybricks project ans realized thay I could also via Bluetooth control make a device and software gui that would not only stream video but also control the driving od the train. 