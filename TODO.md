TODO:  you must poweron the TX before you power on the RX, as the tx is a wifi "access point", and the RX will not be able to "join" the network if its not there.  Similarly, if the TX looses power for any reason, the RX will NOT reconnect, and you need to reboot the RX afterward. 

TODO:  the PPM input pulses from the TX are NOT read using a "timer capture" or interrupt routine, instead via a busy-wait loop.  This can result in some jitter, so we have a dead-band in software to minimise this.  Better to to it right. 

TODO:  there's code in here to allow these wifi devices to have a webserver ( each, etc ), but at the moment, there's just a holding page, and it's not used.  Would be excellent to reconfigure the SSID and password , and other things ,here.

TODO:  there is code to support MDNS and SSDP protocol/s, but neither of these is actively used right now, and neither is properly tested. they might be good to use in the future tho.

TODO:  the bushbutton code only works on the 'nodemcu' dev board that has USB and a push button labeled 'USER'.  need to simplify this and generalise it. in the mean time, just change the AP_OR_STA variable before flashing it.

TODO:  there are more minor code issues in-line, search for 'TODO' below. :-) 


NOTE: the PPM input pulses from the TX itself into the esp8266 can be any number from 8-16 long, but must idle "HIGH" when looked at in oscilloscope. ( this is normal for most PPM )

NOTE: The esp8266 that is in the TX sends UDP packets to the BROADCAST address of the network it's on( 192.168.4.255 ), on port 5554 (no particular reason for this number) , and all RX client/s can listen for these and turn them into PPM output stream.

NOTE: when you connect to the SERIAL interface of these device/s, they give you some debug info about what hte system is doing ( during startup), or a listof all the channel values as they are "live" ( just before being TX'd, or just after being RX'd, as appropriate )
