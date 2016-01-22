This code was developed using the following: 

Arduino IDE version: 1.6.4

AND:

"Arduino core for ESP8266 WiFi chip" 
see https://github.com/esp8266/Arduino
In brief:  
Start Arduino and open Preferences window. 
Enter http://arduino.esp8266.com/stable/package_esp8266com_index.json into Additional Board Manager URLs field.
Open Boards Manager from Tools > Board menu and install esp8266 platform

Depending on the specific ESP8266 based board, you will need to determine how you program it.    

eg1: 
With a pcb labeled 'nodemcu devkit 0.9', this has a USB port, and runs serial-over-usb with a CH340 chip ( it's just like a FTDI, only chinese and needs a different driver ). 
This particular board supports a "reset method" of "nodemcu", and as a result can be programmed just like an Arduino ( no special actions required ).

eg2: 
With a board that does NOT have USB, it will have TX, RX, GND pins ( connect these to your favourite FTDI or similar USB-to-serial cable ) , AND also GPIO0 is important.   
To program these, you need to:
1 - connect GPIO0 to GND
2 - power it on
3 - reflash ( Press Upload) with the Arduino IDE.
4 - disconnect GPIO0 from GND
5 - reboot ( disconnect and reconnect power ) 


https://learn.sparkfun.com/tutorials/esp8266-wifi-shield-hookup-guide/re-programming-the-esp8266
http://iot-playground.com/2-uncategorised/38-esp8266-and-arduino-ide-blink-example


![How to wire the Classic ESP8266](/web/esp8266-reflash.png?raw=true "How to wire the Classic ESP8266")


