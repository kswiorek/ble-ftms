# Arduino BLE Indoor Bike Fitness Machine

This project uses two ESP32 boards to emulate an expensive indoor bike trainer and a power meter.<br/>
<br/>
One ESP measures power and cadence on a crank, the other receives these values, emulates a trainer and forwards the data. It also receives the simulation parameter from a computer and sets position of the resistance screw accordingly.<br/>
The data is also displayed on a 12864 screen and the user is able to change the resistance in ERG mode and the gears in simulation mode.<br/>
The user can also load a file with resistance values and times when to set them on an SD card.<br/>
The whole "realistic physics" part turned out to be more annoying to set up than realistic, so I added a configurator, with which the user can graphicaly define the function converting from the simulated grade to the resistance. More described in the instructable.
<br/><br/>
The promissed instructable <a href="https://www.instructables.com/DIY-Indoor-Bike-Smart-Trainer/?cb=1604870523">https://www.instructables.com/DIY-Indoor-Bike-Smart-Trainer/?cb=1604870523</a>

If you want to dig deaper into the code I highly suggest reading the "Fitness Machine Service" specifications
<a href="https://www.bluetooth.com/specifications/gatt/">https://www.bluetooth.com/specifications/gatt/</a>
