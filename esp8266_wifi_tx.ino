//
// Make two ESP8266 wifi modules into a RC Transmitter and RC Reciever!
// THis same code below can be flashed to BOTH units, with just a 1 line edit to decide which is TX and which is RX.
// Line 248, where variable AP_OR_STA is hard-coded decides if you are programming for a TX ( WIFI_AP ) or RX ( WIFI_STA) 
// 
// TODO:  you must poweron the TX before you power on the RX, as the tx is a wifi "access point", and the RX will not be able to "join" the network if its not there.
// TODO:  the PPM input pulses from the TX are NOT read using a "timer capture" or interrupt routine, instead via a busy-wait loop.  This can result in some jitter, so we have a dead-band in software to minimise this.  Better to to it right. 
// TODO:  there's code in here to allow these wifi devices to have a webserver ( each, etc ), but at the moment, there's just a holding page, and it's not used.  Would be excellent to reconfigure the SSID and password , and other things ,here.
// TODO:  there is code to support MDNS and SSDP protocol/s, but neither of these is actively used right now, and neither is properly tested. they might be good to use in the future tho.
// TODO:  the bushbutton code only works on the 'nodemcu' dev board that has USB and a push button labeled 'USER'.  need to simplify this and generalise it. in the mean time, just change the AP_OR_STA variable before flashing it.
// TODO:  there are more minor code issues in-line, search for 'TODO' below. :-) 

// NOTE: the PPM input pulses from the TX itself into the esp8266 can be any number from 8-16 long, but must idle "HIGH" when looked at in oscilloscope. ( this is normal for most PPM )
// NOTE: The esp8266 that is in the TX sends UDP packets to the BROADCAST address of the network it's on( 192.168.4.255 ), on port 5554 (no particular reason for this number) , and all RX client/s can listen for these and turn them into PPM output stream.
// NOTE: when you connect to the SERIAL interface of these device/s, they give you some debug info about what hte system is doing ( during startup), or a listof all the channel values as they are "live" ( just before being TX'd, or just after being RX'd, as appropriate )

// HARDWARE SETUP:
// PPM IN is wired to the esp8266 in the TX, on GPIO13, or ~D7 ( depending on your labels ) see PPMINPIN define in the code.
// PPM OUT is wired to the esp8266 in the RX, on GPIO12, or ~D6 ( depending on your labels ) see PPMOUTPIN define in the code.
// there's also a debug pin for the oscilloscope called TRIGGERPIN which is GPIO14  or ~D5 ( depending on your labels ) - not used normally. 

// UDP packet format: 
// Inline with the KISS principle, I've just allocated 16bits (two bytes) to each channel in the PPM stream, that's it.    
// So a UDP packet from port 5554 with a 16byte data/payload is used for when we have 8 PPM channels ( 1-8 ).  8 is the MINIMUM supported.  
// Conversely, If the PPM stream from the TX has 16 channels ( 1-16 ), then the UDP packets it generates will have 32bytes of data payload.  16 is the MAXIMUM supported.
// 

// 
// additianl references:
//   https://github.com/jorisplusplus/AR.Drone-ESP8266/blob/master/Drone.ino
//  and many other places.
// 
// (c) David "Buzz" Bussenschutt  20 Jan 2016

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <IPAddress.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
// can remove these if SSDP andor mDNS arent used.
#include <ESP8266SSDP.h>
#include <ESP8266mDNS.h>

/* extern "C" {
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "cont.h"
}
*/

/*
unsigned char softap_stations_cnt;
struct station_info *stat_info;
struct ip_addr *xIPaddress;
uint32 uintaddress;

  needed for WIFI callbacks only.
extern "C" {
  #include "user_interface.h"
  #include "ip_addr.h"
  #include "mem.h"

  void _onWiFiEvent(System_Event_t *event){
    if(event->event == EVENT_SOFTAPMODE_STACONNECTED){
      Event_SoftAPMode_StaConnected_t *data = (Event_SoftAPMode_StaConnected_t *)(&event->event_info.sta_connected);
      Serial.printf("Station Connected: id: %d, mac: " MACSTR "\n", data->aid, MAC2STR(data->mac));
      delay(2000) ; // ot give it time to get an IP
      softap_stations_cnt = wifi_softap_get_station_num();
      stat_info = wifi_softap_get_station_info(); 
      Serial.println(softap_stations_cnt);
    } 
    if(event->event == EVENT_SOFTAPMODE_STADISCONNECTED){
      Event_SoftAPMode_StaDisconnected_t *data = (Event_SoftAPMode_StaDisconnected_t *)(&event->event_info.sta_disconnected);
      Serial.printf("Station Disconnected: id: %d, mac: " MACSTR "\n", data->aid, MAC2STR(data->mac));
    }
    // unfortunately, this even does not work in SOFTAP mode, so it's useless to us. 
    if(event->event == EVENT_STAMODE_GOT_IP){
      Event_StaMode_Got_IP_t *data = (Event_StaMode_Got_IP_t *)(&event->event_info.got_ip);
      Serial.printf("Station got IP: ip: %d \n", data->ip);
    }
    
  }
}
#include "user_interface.h"

*/


// as an AP:
const char *APssid = "WIFI_TX";
const char *APpassword = "password";

// as a client of a real WIFI network:
//const char *ssid = "MY_HOME_NETWORK";
//const char *password = "my_home_network_password";
// as a client of the TX wifi network:
const char *ssid = "WIFI_TX";
const char *password = "password";

// We can send UDP data to specific "connected clients", or we can just BROADCAST it.
// when doing the "specific clients", ATM we only send to IP's that have just done an HTTP request to our server! 
#define BROADCAST_UDP 1
const int navPort = 5554;
WiFiUDP Udp; // this will be the BROADCAST address we send to, if we use it. 

ESP8266WebServer HTTP(80);

MDNSResponder mdns;


volatile int _rising = 0;

// 2 or 12 are probably ok.       2=NODEMCU_D4, 12=NODEMCU_D6 
//#define INTERUPT_PIN 12 
/*
NodeMCU labels table for GPIO
// http://www.14core.com/wp-content/uploads/2015/06/Node-MCU-Pin-Out-Diagram1.png
 ~D6 = 12 /      / HMISO
 ~D7 = 13 / RXD2 / HMOSI
 ~D5 = 14 
 */
 
 // list of currently connected clients ( that are getting a UDP strream sent ot them)
 // max of 4 because 'conf.max_connection = 4' in ESP8266WiFi.cpp
 
 #if BROADCAST_UDP == 0
 IPAddress clients[4];
 int last_client_idx = 0;
 #else
 IPAddress broadcast;
 #endif
 // us, once we know it.
 IPAddress myIP;
 
 // buff for recieving UDP when in STA mode.
 char incoming[1024];

 
void handleHTTP() {
	HTTP.send(200, "text/html", "<h1>You are at the future WiFi TX module config page.</h1>");
        Serial.print("an http client connected, fyi.\n");
        WiFiClient x = HTTP.client();
        #if BROADCAST_UDP == 0
        IPAddress i = x.remoteIP();
        Serial.println(i);
        //  make sure cleint/s are unique 
        if ( ! is_existing(i) ) {
          clients[last_client_idx] = i;
          last_client_idx++;
          Serial.print("NEW UDP client detected ... streaming started! n");
        } else { 
          Serial.print("EXISTING UDP client - streaming already!\n");
        } 
        #endif
}
#if BROADCAST_UDP == 0
bool is_existing(IPAddress x ){ 
  for ( int e = 0 ; e < last_client_idx ; e++ ) { 
      if (clients[e] == x ) return true;
  }
  return false;
} 
#endif

//NON-interrupt based PPMIN, seems pretty good with a processor this fast... 

//TODO WE NEED A BETTER METHOD OF KEEPING THE "channel_count" ( ie the number of pulses in a ppm frame ) in SYNC with the actual DATA we have coming IN
// at teh moment, if the actuals are LESS than the 16, it'll be OK, and auto-shrink, but if the channel counts go back UP ( eg, from 8-16 ) we need to reboot both ends for that to work. ( BAD )

// these two need to match:
unsigned int channel_count = 16; // start assuming 16, we'll automatically move to less ( but no less than 8 ) if we detect we can...? 
// an 'unsigned short' on the esp8266 is TWO bytes each, and we use this as it's max value is 60000, and that's heaps, as we generally should not go over 2000.
// its unsigned, as we don't want weird wrap-arounds to negative numbers to screw us if I got this wrong. 
unsigned short pulselens[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 } ; 


#define PPMINPIN  13
#define PPMOUTPIN  12
#define TRIGGERPIN  14

void ppm_setup() {

 pinMode(PPMOUTPIN, OUTPUT);        //GPIO12 = nodemcu ~D6
 pinMode(TRIGGERPIN, OUTPUT);        //GPIO14 = nodemcu ~D5
 pinMode(PPMINPIN, INPUT);       //GPIO13 = nodemcu ~D7
  /*
  pinMode(14, OUTPUT);   
  pinMode(15, OUTPUT);     
  pinMode(16, OUTPUT); 
 */

}

// nodemcu button is plexed with the LED for some weird reason.
const int buttonPin = 16;
void button_enable()
{
  bool enabled = true;
  pinMode(buttonPin, OUTPUT);
  digitalWrite(buttonPin, enabled ? HIGH : LOW);
  if (enabled) pinMode(buttonPin, INPUT);
}
// nodemcu button is plexed with the LED for some weird reason.
void button_disable()
{
  bool enabled = false;
  pinMode(buttonPin, OUTPUT);
  digitalWrite(buttonPin, enabled ? HIGH : LOW);
  if (enabled) pinMode(buttonPin, INPUT);
}
// nodemcu button is plexed with the LED for some weird reason.
void flash()
{
  for (int i = 0; i < 5; i++)
  {
    button_disable();  // Toggle button makes LED go on/off
    delay(100);
    button_enable();   // Toggle button makes LED go on/off
    delay(100);
  }
}
// nodemcu button is plexed with the LED for some weird reason.
void button_setup()
{
  button_enable();
  flash();
}
//bool wait_for_release = false;
// nodemcu button is plexed with the LED for some weird reason.
bool check_button()
{
  if (digitalRead(buttonPin) == HIGH)
  {
    //wait_for_release = true;
    return false;
  }
  return true;
  //else if (wait_for_release)
  //{
  // wait_for_release = false;

  // flash();
  //}
}
// nodemcu button is plexed with the LED for some weird reason.


// will we be one of the other, default to AP!   if button pressed, we'll be an STA
int AP_OR_STA = WIFI_AP;

//SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
//SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP SETUP
void setup(void) {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Starting");

  //wifi_set_event_handler_cb(_onWiFiEvent);

  ppm_setup(); 
  
  button_setup(); 
  
  if (check_button() ) {  
      AP_OR_STA = WIFI_STA;
      Serial.println("Please release push button ! ");
  }
  // temp TODO HACK, forcong AP mode..
  AP_OR_STA = WIFI_AP;
  
// be an AP
  if ( AP_OR_STA == WIFI_AP ) { 
	Serial.print("Configuring as access point... ");
        Serial.print(" SSID:");
        Serial.print(APssid);        
        Serial.print(" password:");
        Serial.println(APpassword);
        
	/* You can remove the password parameter if you want the AP to be open. */
        WiFi.disconnect();
        WiFi.mode(WIFI_AP); // without this softap can't do multicast udp.
	WiFi.softAP(APssid, APpassword);

	myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);

        //delay(1000); // time for AP to startt up and send beacon..

  } 

  // DONT be an AP
  if ( AP_OR_STA == WIFI_STA ){ 
    	Serial.print("Configuring as Station, NOT AP...");
        // Connect to pre-existing WiFi network with WIFI_STA
        WiFi.disconnect();
        WiFi.mode(WIFI_STA); // WIFI_AP or WIFI_STA
        WiFi.begin(ssid,password );
      
        // Wait for connection
        Serial.println("trying to connect to WIFI.....");
        int tries = 0;
        while ((WiFi.status() != WL_CONNECTED ) && (tries < 50 ) ){
          Serial.println(".....");
          delay(200);
          tries++;
        }
        //WIFI fail
        if (tries >= 30 ) {  
          // REBOOT
          ESP.restart();
        }
        
        Serial.println("Connected!");
        myIP = WiFi.localIP();
	Serial.print("STA IP address: ");
        Serial.println(myIP);
  }
  
  // set these to either '0' or '1'
  #define IS_HTTP_SERVER 1
  #define IS_SSDP_SERVER 0
  #define IS_MDNS_SERVER 0
  
  #if IS_SSDP_SERVER == 1 
  #if IS_HTTP_SERVER == 0 
  #error "can't enable SSDP without HTTP, sorry! "
  #endif
  #endif
  
  
  // be a HTTP server:  - not totally critical for this, but it's working, so we might use it later for config.
  if ( IS_HTTP_SERVER ) {
	HTTP.on("/", handleHTTP);
        HTTP.on("/index.html", handleHTTP); // same as above
	Serial.println("HTTP server started");
  }
  
  // be a SSDP server too: 
  if ( IS_SSDP_SERVER ) { 
      HTTP.on("/description.xml", HTTP_GET, [](){
      SSDP.schema(HTTP.client());
      });
  } 
  if ( IS_SSDP_SERVER || IS_HTTP_SERVER ) { 
      HTTP.begin();
  } 
  
  //TODO TEST IF SSDP AND MDNS is actually working as it should.  is is sending packets etc?  
  if ( IS_SSDP_SERVER == 1 ) { 
    Serial.print("Starting SSDP...\n");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("ESP8266 WIFI TX");
    SSDP.setSerialNumber("001122334455");
    SSDP.setURL("index.html");
    SSDP.setModelName("Buzzs ESP8266 WIFI TX 2016");
    SSDP.setModelNumber("001122334455");
    SSDP.setModelURL("http://github.com/davidbuzz/");
    SSDP.setManufacturer("David / Buzz");
    SSDP.setManufacturerURL("http://github.com/davidbuzz/");
    SSDP.begin();
    Serial.print("SSDP done.\n");

  }
  
  // mDNS
  char hostname [12+1];
  strcpy (hostname,"esp8266");
  if ( IS_MDNS_SERVER == 1) { 
    if (mdns.begin(hostname)) {
    Serial.println("MDNS responder started");
    } else { 
    Serial.println("MDNS responder FAILED to start");
    }
  }
  
  
  Serial.println("Other Nearby WIFI networks, just a FYI... ");  
  // review wifi info... scan takes approx 2 secs...
  String _ssid;
  uint8_t encryptionType;
  int32_t RSSI;
  uint8_t* BSSID;
  int32_t channel;
  bool isHidden;
  int netcount = WiFi.scanNetworks();
  for (int n = 0; n < netcount; n++) {
    WiFi.getNetworkInfo(n, _ssid, encryptionType, RSSI, BSSID, channel, isHidden);
    Serial.println(String("SSID : ") + _ssid);
    Serial.println(String("encryptionType : ") + encryptionType);
    Serial.println(String("RSSI : ") + RSSI);
    Serial.println(String("Channel : ") + channel);
    Serial.println(String("Hidden : ") + isHidden);
    Serial.println();
  }
  
  // from out IP, determine broadcast IP , assuming class C ( /24 ) network.
  #if BROADCAST_UDP == 1
    broadcast = myIP;
    broadcast[3] = 255;
  #endif
  
  // we listen, all the time, for UDP data coming IN.
  if ( AP_OR_STA == WIFI_STA ){
        Udp.begin(navPort); //Open port for navdata
        Udp.flush();
  } 

  if ( AP_OR_STA == WIFI_AP ){
    Serial.println("Starting main loop ( Listening for PPMIN on GPIO13, aka ~7 on a modemcu ) "); // see PPMINPIN
  } else { 
    Serial.println("Starting main loop ( Waiting for UDP on 5554, and PPMOUT on GPIO12, aka ~6 on a modemcu ) "); // see PPMOUTPIN
  } 
  
}


inline short read_pulse_len(bool trace){
  int x = 0 ;
  // wait for edge to RISE
  while ( x == 0 ) { 
    x = digitalRead(PPMINPIN ) ; 
    ESP.wdtFeed(); // in the event we don't have good PPM pulses, we can hang here, but we don't want to crash/reboot.
  } 
  if ( trace ) digitalWrite(PPMOUTPIN,x);  // for DEBUG ONLY.
  short risestart = micros();
  
    // wait for edge to FALL
  while ( x == 1 ) { 
    x = digitalRead(PPMINPIN ) ;
    ESP.wdtFeed(); // in the event we don't have good PPM pulses, we can hang here, but we don't want to crash/reboot.
  }
  if ( trace ) digitalWrite(PPMOUTPIN,x);  // for DEBUG ONLY.
  //delayMicroseconds(50);      // for debug only  - so last/8th channel shows up on trace better.
  // if rise length > some amount, continue
  short riseend = micros();
  
  short risetime = ( riseend - risestart );
 return  risetime;
}




// if there's been any change in the data, and we'll be formulating a new UDP packet..
int udpnew = 0;

// FYI: PPM frames repeat approx every 20-22ms, so 50frames = approx 1sec., so we'll send one UDP every 25 frames for 2hz
int recentframes = 0;


unsigned long sta_runtime = micros();


//LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
//LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP LOOP
void loop(void) {

  HTTP.handleClient(); // http server requests need handling... 
  
 if ( AP_OR_STA == WIFI_AP ) {
  mdns.update();
  //ESP.wdtFeed(); // prevent watchdog
  
  
  // FIRST THING WE DO IS LOOK FOR THE SPACING BETWEEN THE PPM FRAMES. ITS a HIGH voltage of around 4000us. so anything over 3000 is considered good.
  #define MINFRAMESPACEING 3000
  short risetime = read_pulse_len(false); 
  if ( risetime > MINFRAMESPACEING  ) { 
    
     recentframes++;
     //Serial.println("sync pulse ok!"); 
     //Serial.println(risetime); 
     
     // sync pulse out at the start, for the oscilloscope to trigger on easily...
     digitalWrite(PPMOUTPIN, 0); 
     digitalWrite(TRIGGERPIN, 1); 
     delayMicroseconds(50);  // a 1000us pulse is nice and easy for my scope to read at many scales, but messes with other things, as small as 50us works at some scales.
     digitalWrite(TRIGGERPIN, 0); 
     delayMicroseconds(200); 
     digitalWrite(PPMOUTPIN, 1); 
        
      // once we've got the sync spacing, we read the next 8 pulse lengths..
      // and IF there's any real difference from the previous one, we'll flag to
      // send the UDP packet later.
      #define DEJITTER 5
      for ( int p = 0 ; p < channel_count ; p++)  {
        short pnew = read_pulse_len(true); 
        if ( p == 0 && pnew > 6000 ) {  pnew = read_pulse_len(true);  } // if it's the first pulse, and we've read an overly LONG one, just drop it and try again.
        if  ( pnew > 6000 ) {   if ( channel_count > 8 ) channel_count--;  break; }  // shrink the channel count from 16 down to the actual channel count, if needed.
        if ( abs( pnew - pulselens[p] ) > DEJITTER ) {
          // flag to say there's changed data to send...
          udpnew++;
          pulselens[p] = pnew;
          //Serial.print(pulselens[p]); Serial.print("\t"); 
        } 
      }
      //Serial.println(); 

      // after last pulse is read ( and copied to TRIGGERPIN ), delay here a bit to finish last pulse in frame.
      delayMicroseconds(400);      // for debug only  - so last/8th channel shows up on trace better.      
      
      // in the event that the TX stick are unchanged, we'll continue to set at least ONE UDP packet per ~second
       if ( recentframes > 15  ) {  // 15 is kinda good approx 2-3 heatbeats/sec .. in theory: 25 ~ 2Hz, 50 ~ 1hz
        udpnew++;
        recentframes = 0;
      } 
      
      // comment out to stop UDP packets actually being sent:
      #define SEND_UDP 1
      
      // HINT simple way to decode 8 byte udp packets with this linux/osx command: 
      // this first one does NOT work for broadcast packets:
      // nc -l -k -u 5554 | xxd
      // or:  ( which works for broadcast packets ) 
      // sudo tcpdump -n 'udp port 5554'
      #if SEND_UDP == 1
      if ( udpnew > 0 ) { 
        // EITHER THIS:
        #if BROADCAST_UDP == 0
        // for each of the connected clients, send them a UDP packet
        for ( int cl = 0 ; cl < last_client_idx ; cl++ ) { 
          Udp.beginPacket(clients[cl], navPort);
          for(int q = 0; q < channel_count ; q++) {
            Udp.write(pulselens[q]);
//            Serial.println(pulselens[q]); 
          }
          Udp.endPacket();
        }
        #endif
        // OR THIS:
        #if BROADCAST_UDP == 1
          //Serial.print(".");
          Udp.beginPacket(broadcast, navPort);
          for(int q = 0; q < channel_count ; q++) {
            if (pulselens[q] > 4000 ) {  Serial.print("\nEEK EEK EEK EEK EEK EEk !!!!!!!!"); break;  } 
            // each of our readings is stored in a 'stort' ( two bytes), so we'll send them over the wire as two bytes, giving us 16bit res/channel in theory.
            char lb = (byte)(pulselens[q] >> 8); 
            char hb = (byte)(pulselens[q]); 
            Udp.write(lb);
            Udp.write(hb);
            Serial.print(pulselens[q]); Serial.print("\t"); 
          }
          Udp.endPacket();
          Serial.println(""); 
        #endif
        
        udpnew = 0; 
      }
      #endif
        
        //delay(10); this will 1/2 the number of ppm frames we see and action ( we'll do every second one )     
  }  // the end of the PPMIN->UDP read loop! 
 } 
  
  // by enabling this, we are no longer reading EVERY PPM frame, only every second one, as we're WRITING this when we should be readin :-( 
 if ( AP_OR_STA == WIFI_STA  ) { 
   
   
   // wait for UDP packet on port 5554, then break it open and use it. :-) 
    //Udp.beginPacket(drone, navPort);
    //Udp.write(0x01);
    //Udp.endPacket();
    if(Udp.parsePacket()) {
      int len = Udp.read(incoming, 1024);
      //Serial.print("packet len:");
      //Serial.println(len);
      // TODO if len != 16 skip all this, as it's a dud packet.
      //
      incoming[len] = 0;   
      for ( int b = 0 ; b < channel_count ; b ++ ) { 
         byte t1 = incoming[b*2];
         byte t2 = incoming[b*2+1];
         short s = (short)(t1 << 8)  | (short)t2;        
        Serial.print("\t");
        Serial.print(s);
        if (s > 4000 ) {  Serial.print("\nEEK 4000 EEK EEK EEK EEK EEk !!!!!!!!"); break;  } 
        if (s < 400 ) {  Serial.print("\nEEK 400 EEK EEK EEK EEK EEk !!!!!!!!"); channel_count--; break;  } 
         pulselens[b] = s;
      } 
      
      Serial.println();

    }  
    // TODO is this calc OK?   it's kinda ordinary to re-calc this constantly.
    static int frame_time_total = 20000;
    if (channel_count == 8 )   frame_time_total = 20000;
    if (channel_count > 8 && channel_count < 16 )   frame_time_total = 20000+((40000-20000)/8*(channel_count-8));
    if (channel_count == 16 )   frame_time_total = 40000;
    
    //Serial.print("frame time:");
    //Serial.println(frame_time_total);
    
    
    // if it's been at least 20ms(8ch) or 40ms(16ch) since we last send a PPM steam out ( the entire frame is 20ms ) 
    if ( micros() - sta_runtime  >  frame_time_total) { 
      sta_runtime = micros(); 
      
      digitalWrite(TRIGGERPIN, 1); 
      delayMicroseconds(50);  // a 1000us pulse is nice and easy for my scope to read at many scales, but messes with other things, as small as 50us works at some scales.
      digitalWrite(TRIGGERPIN, 0); 
     
      //start = 0; 
      // write PPM pulses..
      digitalWrite(PPMOUTPIN,HIGH); // first start with the line HIGH during the inter-frame
      delayMicroseconds(4000); // now do the inter-fram delay.  it's got to be around 4000, but over 3000
      for(int q = 0; q < channel_count ; q++) {
        digitalWrite(PPMOUTPIN,LOW);
        delayMicroseconds(500); // some low fixed about
        digitalWrite(PPMOUTPIN,HIGH);
        delayMicroseconds(pulselens[q]); // delay the actual length of teh pulse, and we are done. 
      }
      // and to finish the last pulse off: 
      digitalWrite(PPMOUTPIN,LOW);
      delayMicroseconds(500); // some low fixed about
      digitalWrite(PPMOUTPIN,HIGH);
    }
 } 
  
}


void print_binary(int v, int num_places)
{
    int mask=0, n;

    for (n=1; n<=num_places; n++)
    {
        mask = (mask << 1) | 0x0001;
    }
    v = v & mask;  // truncate v to specified number of places

    while(num_places)
    {

        if (v & (0x0001 << num_places-1))
        {
             Serial.print("1");
        }
        else
        {
             Serial.print("0");
        }

        --num_places;
        if(((num_places%4) == 0) && (num_places != 0))
        {
            Serial.print("_");
        }
    }
}

void print_hex(int v, int num_places)
{
    int mask=0, n, num_nibbles, digit;

    for (n=1; n<=num_places; n++)
    {
        mask = (mask << 1) | 0x0001;
    }
    v = v & mask; // truncate v to specified number of places

    num_nibbles = num_places / 4;
    if ((num_places % 4) != 0)
    {
        ++num_nibbles;
    }

    do
    {
        digit = ((v >> (num_nibbles-1) * 4)) & 0x0f;
        Serial.print(digit, HEX);
    } while(--num_nibbles);

}

/*   BONUS, FOR THOSE THAT READ THE CODE! 

udp_reciever.py


#!/usr/bin/env python
# Make two ESP8266 wifi modules into a RC Transmitter and RC Reciever!
# The below test/example python code is capable of receiving the UDP packets that the TX code generates, and printing the data to the console/screen. that's all.
# You just need to put your laptop/whatever on the same access point as the TX ( its SSID is WIFI_TX by default in the code ).
# (c) David "Buzz" Bussenschutt  20 Jan 2016
import socket
import binascii
import struct

port = 5554
client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
client_socket.bind(('',port))  # listen to all interfaces
#client_socket.bind(('192.168.4.255',port))  # listen to broadcast only
client_socket.setblocking(False)

while ( 1 ):
    try:
                data,address = client_socket.recvfrom(10000)
    except socket.error:
                pass
    else:
	_len = len(data)
	#print "data recv :", data," len:",_len
	# 8 channels = 16 bytes, etc...
	if _len == 16: 
		( p1,p2,p3,p4,p5,p6,p7,p8) =  struct.unpack(">HHHHHHHH",data)
		print str(p1) + "\t" + str(p2) + "\t" +str(p3) + "\t" +str(p4) + "\t" +str(p5) + "\t" +str(p6) + "\t" +str(p7) + "\t" +str(p8) 
	if _len == 20: 
		( p1,p2,p3,p4,p5,p6,p7,p8,p9,p10) =  struct.unpack(">HHHHHHHHHH",data)
		print str(p1) + "\t" + str(p2) + "\t" +str(p3) + "\t" +str(p4) + "\t" +str(p5) + "\t" +str(p6) + "\t" +str(p7) + "\t" +str(p8) + "\t" +str(p9) + "\t" +str(p10)
	if _len == 24: 
		( p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12) =  struct.unpack(">HHHHHHHHHHHH",data)
		print str(p1) + "\t" + str(p2) + "\t" +str(p3) + "\t" +str(p4) + "\t" +str(p5) + "\t" +str(p6) + "\t" +str(p7) + "\t" +str(p8) + "\t" +str(p9) + "\t" +str(p10)+ "\t" +str(p11) + "\t" +str(p12)
	if _len == 28: 
		( p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14) =  struct.unpack(">HHHHHHHHHHHHHH",data)
		print str(p1) + "\t" + str(p2) + "\t" +str(p3) + "\t" +str(p4) + "\t" +str(p5) + "\t" +str(p6) + "\t" +str(p7) + "\t" +str(p8) + "\t" +str(p9) + "\t" +str(p10)+ "\t" +str(p11) + "\t" +str(p12)+ "\t" +str(p13) + "\t" +str(p14)
	if _len == 32: 
		( p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15,p16) =  struct.unpack(">HHHHHHHHHHHHHHHH",data)
		print str(p1) + "\t" + str(p2) + "\t" +str(p3) + "\t" +str(p4) + "\t" +str(p5) + "\t" +str(p6) + "\t" +str(p7) + "\t" +str(p8) + "\t" +str(p9) + "\t" +str(p10)+ "\t" +str(p11) + "\t" +str(p12)+ "\t" +str(p13) + "\t" +str(p14)+ "\t" +str(p15) + "\t" +str(p16)




*/

