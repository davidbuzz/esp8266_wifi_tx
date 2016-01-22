# esp8266_wifi_tx
Make two ESP8266 wifi modules into a RC Transmitter and RC Reciever!

THis same code can be flashed to BOTH units, with just a 1 line edit to decide which is TX and which is RX.
Line ( approx ) 248, where variable AP_OR_STA is hard-coded decides if you are programming for a TX ( WIFI_AP ) or RX ( WIFI_STA) 

HARDWARE SETUP:

PPM IN is wired to the esp8266 in the TX, on GPIO13, or ~D7 ( depending on your labels ) see PPMINPIN define in the code.

PPM OUT is wired to the esp8266 in the RX, on GPIO12, or ~D6 ( depending on your labels ) see PPMOUTPIN define in the code.

There's also a debug pin for the oscilloscope called TRIGGERPIN which is GPIO14  or ~D5 ( depending on your labels ) - not used normally. 

UDP packet format: 

Inline with the KISS principle, I've just allocated 16bits (two bytes) to each channel in the PPM stream, that's it.    

So a UDP packet from port 5554 with a 16byte data/payload is used for when we have 8 PPM channels ( 1-8 ).  8 is the MINIMUM supported.  

Conversely, If the PPM stream from the TX has 16 channels ( 1-16 ), then the UDP packets it generates will have 32bytes of data payload.  16 is the MAXIMUM supported.

see also TODO file.

![Classic ESP8266 module](/web/esp8266.jpg?raw=true "Classic ESP8266 module")

![Example Dev PCB with ESP8266 module](/web/es-l500.jpg?raw=true "Example Dev PCB with ESP8266 module]")

[See a video of the prototype here!](https://www.youtube.com/watch?v=6-0GJ21oUO4&spfreload=10)

(c) David "Buzz" Bussenschutt  20 Jan 2016