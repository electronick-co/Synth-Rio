/*  Synthrio
 *   Work of art
 *   Julian Henao - Artist
 *   Nick Velasquez - HW
 *   Made by: Nick Velasquez
 *   2022
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "esp32-hal-cpu.h"

#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <tables/cos8192_int8.h>
#include <mozzi_midi.h>
#include <Smooth.h>
#include <AutoMap.h> // maps unpredictable inputs to a range


/*-------------------------------------------------------------------------------------------------------------------------                     

                    ██    ██  █████  ██████  ██  █████  ██████  ██      ███████ ███████ 
                    ██    ██ ██   ██ ██   ██ ██ ██   ██ ██   ██ ██      ██      ██      
                    ██    ██ ███████ ██████  ██ ███████ ██████  ██      █████   ███████ 
                     ██  ██  ██   ██ ██   ██ ██ ██   ██ ██   ██ ██      ██           ██ 
                      ████   ██   ██ ██   ██ ██ ██   ██ ██████  ███████ ███████ ███████ 
-------------------------------------------------------------------------------------------------------------------------*/                                                              
                                                                    
// --------------General Variables --------------                                                                
// LED Pin
const int ledPin = 2;
const int led_hb_short = 300;
const int led_hb_long = 2000;
int led_heartbeat = led_hb_short;


// -------------------------Mozzi Variables-----------------------------------------
// control variable, use the smallest data size you can for anything used in audio
int audio_parameters[4]={random(1024),random(1024),random(1024),random(1024)};
int gain = 255;
int freq = 440;

// ------Sensor to values mapping-------

// desired carrier frequency max and min, for AutoMap
int MIN_CARRIER_FREQ = random(5, 200);
int MAX_CARRIER_FREQ = random(300,550);

// desired intensity max and min, for AutoMap, note they're inverted for reverse dynamics
int MIN_INTENSITY = random(100, 400);
int MAX_INTENSITY = random(10, 50);

// desired mod speed max and min, for AutoMap, note they're inverted for reverse dynamics
int MIN_MOD_SPEED = random(500,10000);
int MAX_MOD_SPEED = random(1, 100);


AutoMap kMapCarrierFreq(0,1023,MIN_CARRIER_FREQ,MAX_CARRIER_FREQ);
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);



// ----------------------------MQTT Variables-------------------------------------
String device_id;

// ---------------------------Automata variables---------------------------------
byte p1_dir = 1;
byte p2_dir = 1;
byte p3_dir = 1;
int randNum;
const int lower_random = 20;
const int higer_random = 1000;

//----------------------------Look Hook variables-----------------------
uint32_t mqtt_time = millis();
uint32_t keepalive_time = millis();
uint32_t led_hb_time = millis();
uint32_t automata_time = millis();
uint32_t inner_automata_time = millis();

const int mqttHook_timeout = 20;
const int keepAliveHook_timeout = 10000;
const int automataHook_timeout = 30000;
const int automataHook_inner_timeout = 120;

/*-------------------------------------------------------------------------------------------------------------------------                     
                                      
                                      ███    ███  ██████  ████████ ████████ 
                                      ████  ████ ██    ██    ██       ██    
                                      ██ ████ ██ ██    ██    ██       ██    
                                      ██  ██  ██ ██ ▄▄ ██    ██       ██    
                                      ██      ██  ██████     ██       ██    
                                                     ▀▀                     
-------------------------------------------------------------------------------------------------------------------------*/                                                              

// Wifi Credentials
const char* ssid = "SYNTHRIO";
const char* password = "armatostre";

// MQTT configuration
const char* mqtt_server = "192.168.3.71";

String base_topic = "SynthRio";
String subtopic_led;
String subtopic_data;
String subtopic_config;
String subtopic_all_led;
String subtopic_all_data;
String subtopic_all_config;
String pubtopic_general;
char * pubtopic_register = "SynthRio/register";
char * pubtopic_keepalive = "SynthRio/keepalive";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Wait for wifi to be connected
  uint32_t notConnectedCounter = 0;
  Serial.println("Wifi connecting");
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
      digitalWrite(ledPin, !digitalRead(ledPin));
      notConnectedCounter++;
      if(notConnectedCounter > 100) { // Reset board if not connected after 5s
          Serial.println("Resetting due to Wifi not connecting...");
          ESP.restart();
      }
  }
  
  digitalWrite(ledPin, HIGH);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Restart automata time when a message is received.
  automata_time = millis();

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == subtopic_led) {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
  else if (String(topic) == subtopic_data || String(topic) == subtopic_all_data) 
  {
      char *ptr = NULL;
      
      
      byte index = 0;
       ptr = strtok((char *)messageTemp.c_str(), ",");  // delimiter
       while (ptr != NULL)
       {
          audio_parameters[index] = String(ptr).toInt();
          //Serial.println(ptr);
          index++;
          ptr = strtok(NULL, ",");
       }
    
      freq = audio_parameters[0];
      gain = audio_parameters[1];
//    Serial.println(freq);
  }
  else if (String(topic) == subtopic_config || String(topic) == subtopic_all_config) 
  {
    gain = messageTemp.toInt();
    Serial.println(gain);
  }
}

void reconnect() {
  // Loop until we're reconnected
  //while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(device_id.c_str())) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(subtopic_led.c_str());
      client.subscribe(subtopic_data.c_str());
      client.subscribe(subtopic_config.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  //}
}

/*-------------------------------------------------------------------------------------------------------------------------                     
                                    ███    ███  ██████  ███████ ███████ ██ 
                                    ████  ████ ██    ██    ███     ███  ██ 
                                    ██ ████ ██ ██    ██   ███     ███   ██ 
                                    ██  ██  ██ ██    ██  ███     ███    ██ 
                                    ██      ██  ██████  ███████ ███████ ██ 
-------------------------------------------------------------------------------------------------------------------------*/                                                              
                                       

// ------------------------------------- Sine Wave --------------------------------------------
// use: Oscil <table_size, update_rate> oscilName (wavetable), look in .h file of table #included above
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aVibrato(SIN2048_DATA);

//--------------------------------------3 variable FMSynth------------------------------------

Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kIntensityMod(COS2048_DATA);

int mod_ratio = 20; // brightness (harmonics)
long fm_intensity; // carries control info from updateControl to updateAudio

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth <long> aSmoothIntensity(smoothness);

//--------------------------------------<Light Temperature> Multi Oscil------------------------------------
//#define CONTROL_RATE 256
//#define NUM_VOICES 8
//#define THRESHOLD 10
//
//// harmonics
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos1(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos2(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos3(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos4(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos5(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos6(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos7(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, AUDIO_RATE> aCos0(COS8192_DATA);
//
//// volume controls
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol1(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol2(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol3(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol4(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol5(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol6(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol7(COS8192_DATA);
//Oscil<COS8192_NUM_CELLS, CONTROL_RATE> kVol0(COS8192_DATA);
//
//// audio volumes updated each control interrupt and reused in audio till next control
//char v1,v2,v3,v4,v5,v6,v7,v0;
//
//// notes to play depending on whether temperature reading increases or decreases
//float upnotes[NUM_VOICES] = {
//  mtof(62.f),mtof(64.f),mtof(70.f),mtof(72.f),mtof(79.f),mtof(81.f), mtof(86.f), mtof(88.f)};
//
//float downnotes[NUM_VOICES] = {
//  mtof(64.f),mtof(65.f),mtof(88.f),mtof(72.f),mtof(79.f),mtof(84.f),mtof(86.f),mtof(89.f)};



//// returns freq
//int temperatureToFreq(char oscil_num, int temperature){
//  static int previous_temperature;
//  int freq;
//  if (temperature>previous_temperature){
//    freq = upnotes[oscil_num];
//  } else {
//     freq = downnotes[oscil_num];
//  }
//  previous_temperature = temperature;
//  return freq;
//}



void updateControl(){
  // ------------------------------------- Sine Wave --------------------------------------------
//  aSin.setFreq(freq);

  //--------------------------------------3 variable FMSynth------------------------------------

  // map the knob to carrier frequency
  int carrier_freq = kMapCarrierFreq(audio_parameters[0]);

  //calculate the modulation frequency to stay in ratio
  int mod_freq = carrier_freq * mod_ratio;

  // set the FM oscillator frequencies
  aCarrier.setFreq(carrier_freq);
  aModulator.setFreq(mod_freq);

  int LDR1_calibrated = kMapIntensity(audio_parameters[1]);

  // calculate the fm_intensity
  fm_intensity = ((long)LDR1_calibrated * (kIntensityMod.next()+128))>>8; // shift back to range after 8 bit multiply

  // use a float here for low frequencies
  float mod_speed = (float)kMapModSpeed(audio_parameters[2])/1000;
  kIntensityMod.setFreq(mod_speed);

  //--------------------------------------<Light Temperature> Multi Oscil------------------------------------

//  static float previous_pulse_freq;
//
//  // read analog inputs
//  int temperature = audio_parameters[0];
//  int light = audio_parameters[1];
//
//  // map light reading to volume pulse frequency
//  float pulse_freq = light;
//  previous_pulse_freq = pulse_freq;
//
//  v0 = kVol0.next();
//  v1 = kVol1.next();
//  v2 = kVol2.next();
//  v3 = kVol3.next();
//  v4 = kVol4.next();
//  v5 = kVol5.next();
//  v6 = kVol6.next();
//  v7 = kVol7.next();
//
//  // set one note oscillator frequency each time (if it's volume is close to 0)
//  static char whoseTurn;
//  switch(whoseTurn){
//  case 0:
//    kVol0.setFreq(pulse_freq);
//    if(abs(v0)<THRESHOLD) aCos0.setFreq(temperatureToFreq(0,temperature));
//    break;
//
//  case 1:
//    kVol1.setFreq(pulse_freq);
//    if(abs(v1)<THRESHOLD) aCos1.setFreq(temperatureToFreq(1,temperature));
//    break;
//
//  case 2:
//    kVol2.setFreq(pulse_freq);
//    if(abs(v2)<THRESHOLD) aCos2.setFreq(temperatureToFreq(2,temperature));
//    break;
//
//  case 3:
//    kVol3.setFreq(pulse_freq);
//    if(abs(v3)<THRESHOLD) aCos3.setFreq(temperatureToFreq(3,temperature));
//    break;
//
//  case 4:
//    kVol4.setFreq(pulse_freq);
//    if(abs(v4)<THRESHOLD) aCos4.setFreq(temperatureToFreq(4,temperature));
//    break;
//
//  case 5:
//    kVol5.setFreq(pulse_freq);
//    if(abs(v5)<THRESHOLD) aCos5.setFreq(temperatureToFreq(5,temperature));
//    break;
//
//  case 6:
//    kVol6.setFreq(pulse_freq);
//    if(abs(v6)<THRESHOLD) aCos6.setFreq(temperatureToFreq(6,temperature));
//    break;
//
//  case 7:
//    kVol7.setFreq(pulse_freq);
//    if(abs(v7)<THRESHOLD) aCos7.setFreq(temperatureToFreq(7,temperature));
//    break;
//
//  }
//
//    if(++whoseTurn>=NUM_VOICES) whoseTurn = 0;
}
  



int updateAudio(){
  // ------------------------------------- Sine Wave --------------------------------------------
//  Q15n16 vibrato = (Q15n16) 255 * aVibrato.next();
//  //return (aSin.phMod(vibrato) * gain)>>8; // shift back to STANDARD audio range, like /256 but faster
//  return (aSin.next()* gain)>>8; // shift back to STANDARD audio range, like /256 but faster

  //--------------------------------------3 variable FMSynth------------------------------------

  long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
  return MonoOutput::from8Bit(aCarrier.phMod(modulation));

  //--------------------------------------<Light Temperature> Multi Oscil------------------------------------

//  long asig = (long)
//    aCos0.next()*v0 +
//      aCos1.next()*v1 +
//      aCos2.next()*v2 +
//      aCos3.next()*v3;
////      aCos4.next()*v4 +
////      aCos5.next()*v5 +
////      aCos6.next()*v6 +
////      aCos7.next()*v7;
//  asig >>= 8; // shift back to audio output range
//  return (int) asig;
  
}

void mozzi_setup()
{
  //Mozzi Setup
  startMozzi(); // start with default control rate of 64
//  aSin.setFreq(440); // set the frequency
//  aVibrato.setFreq(0);
}

/*-------------------------------------------------------------------------------------------------------------------------                     
                                      ███████ ███████ ████████ ██    ██ ██████  
                                      ██      ██         ██    ██    ██ ██   ██ 
                                      ███████ █████      ██    ██    ██ ██████  
                                           ██ ██         ██    ██    ██ ██      
                                      ███████ ███████    ██     ██████  ██      
-------------------------------------------------------------------------------------------------------------------------*/                                                              

//This function assing the id of the ESP32 based on the MAC address
void id_assign()
{
  String mac = WiFi.macAddress();
  Serial.println(mac);

  if(mac.equals("0C:8B:95:76:61:B0"))
  {
    device_id = "cauca";
  }
  else if (mac.equals("58:BF:25:9F:CB:70"))
  {
    device_id = "magdalena";
  }
  else if (mac.equals("0C:DC:7E:62:14:64"))
  {
    device_id = "amazonas";
  }
  else if (mac.equals("0C:DC:7E:61:54:A8"))
  {
    device_id = "orinoco";
  }
  else if (mac.equals("0C:DC:7E:61:FA:04"))
  {
    device_id = "putumayo";
  }
  else if (mac.equals("0C:DC:7E:61:26:EC"))
  {
    device_id = "meta";
  }
  else if (mac.equals("0C:8B:95:76:13:BC"))
  {
    device_id = "atrato";
  }
  else if (mac.equals("0C:DC:7E:62:BE:EC"))
  {
    device_id = "sinu";
  }
  else //No esta en la lista
  {
    device_id = "nilo";
  }
  
  Serial.print("This device ID is: ");
  Serial.println(device_id);
  
}

void setup(){
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  Serial.println(WiFi.macAddress());

  //Check what ID is this device and change topics based on that
  id_assign(); 
  subtopic_data = base_topic + "/" + device_id + "/" + "data";
  subtopic_config = base_topic + "/" + device_id + "/" + "config";
  subtopic_led = base_topic + "/" + device_id + "/" + "led";
  subtopic_all_data = base_topic + "/all/" + "data";
  subtopic_all_config = base_topic + "/all/" + "config";
  subtopic_all_led = base_topic + "/all/" + "led";
  pubtopic_general = base_topic + "/" + device_id;

  
  //Wifi and MQTT setup
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();

  //Send the infomation of the device to the mqtt server
  String reg_json = "{\"device_id\": \"" + device_id + "\", \"mac\": \"" + WiFi.macAddress() + "\", \"ip\": \"" \
                  + WiFi.localIP().toString() + "\"}";
  
  client.publish(pubtopic_register,reg_json.c_str());

  // Mozzi
  mozzi_setup();
}

/*-------------------------------------------------------------------------------------------------------------------------                                       
                    ██       ██████   ██████  ██████      ██   ██  ██████   ██████  ██   ██ ███████ 
                    ██      ██    ██ ██    ██ ██   ██     ██   ██ ██    ██ ██    ██ ██  ██  ██      
                    ██      ██    ██ ██    ██ ██████      ███████ ██    ██ ██    ██ █████   ███████ 
                    ██      ██    ██ ██    ██ ██          ██   ██ ██    ██ ██    ██ ██  ██       ██ 
                    ███████  ██████   ██████  ██          ██   ██  ██████   ██████  ██   ██ ███████ 
-------------------------------------------------------------------------------------------------------------------------*/                                                                        



// Review MQTT status
void mqttHook()
{
  // Test MQTT status
  if ( (millis()-mqtt_time)  > mqttHook_timeout) 
  {
    mqtt_time = millis();
    client.loop();
 
    if (!client.connected()) 
    {
      //ToDo - Solve problem that when reconnecting audio dies...
      reconnect();
    }
   }
}

// Show LED hearbeat
void heartbeatHook()
{
  // LED heartbeat
  if ( (millis() - led_hb_time) > led_heartbeat)
  {
    led_hb_time = millis();
    
    if ( led_heartbeat == led_hb_short)
    {
      led_heartbeat = led_hb_long;
    }
    else
    {
        led_heartbeat = led_hb_short;
    }
    digitalWrite(ledPin, !digitalRead(ledPin));
  }
}

// Send keep alive to MQTT broker
void keepAliveHook()
{
  // Send keep alive for verification
  if ( (millis() - keepalive_time) > keepAliveHook_timeout)
  {
    keepalive_time = millis();
    String msg = device_id + ":\t" + millis();
    client.publish(pubtopic_keepalive,msg.c_str());
  }
}

// If there is no activity after certain time, move the audio parameters automatically.
void automataHook()
{
  // Automata
  if ( (millis() - automata_time) > automataHook_timeout)
  {
    if ( (millis() - inner_automata_time) > automataHook_inner_timeout)
    {
      inner_automata_time = millis();

      //Serial.println("Automata!!");
      digitalWrite(ledPin, !digitalRead(ledPin));

      // Generate random seqences to increase, decrease or remain the same the parameters.
      randNum = random(1000);
      if(randNum < lower_random)
      {
        p1_dir++;
        if(p1_dir>=3)
        {
          p1_dir = 0;
        }
      }
      randNum = random(1000);
      if(randNum < lower_random)
      {
        p2_dir++;
        if(p2_dir>=3)
        {
          p2_dir = 0;
        }
      }
      randNum = random(1000);
      if(randNum < lower_random)
      {
        p3_dir++;
        if(p3_dir>=3)
        {
          p3_dir = 0;
        }
      }
      
      // If dir is 0, parameter decreases. if 1, increases, if 2, remains the same.
      if (p1_dir == 1)
      {
        audio_parameters[0]+=1;
        if( audio_parameters[0] > 1024)
        {
          audio_parameters[0] = 1024;
          p1_dir = 0;
        }
      }
      else if(p1_dir == 0)
      {
        audio_parameters[0]-=1;
        if( audio_parameters[0] < 0)
        {
          audio_parameters[0] = 0;
          p1_dir = 1;
        }
      }

      if (p2_dir == 1)
      {
        audio_parameters[1]+=1;
        if( audio_parameters[1] > 1024)
        {
          audio_parameters[1] = 1024;
          p2_dir = 0;
        }
      }
      else if (p2_dir == 0)
      {
        audio_parameters[1]-=1;
        if( audio_parameters[1] < 0)
        {
          audio_parameters[1] = 0;
          p2_dir = 1;
        }
      }

      if (p3_dir == 1)
      {
        audio_parameters[2]+=1;
        if( audio_parameters[2] > 1024)
        {
          audio_parameters[2] = 1024;
          p3_dir = 0;
        }
      }
      else if (p3_dir == 0)
      {
        audio_parameters[2]-=1;
        if( audio_parameters[2] < 0)
        {
          audio_parameters[2] = 0;
          p3_dir = 1;
        }
      }
     
      
    }    
  }
}


/*-------------------------------------------------------------------------------------------------------------------------                     
                          ███    ███  █████  ██ ███    ██     ██       ██████   ██████  ██████  
                          ████  ████ ██   ██ ██ ████   ██     ██      ██    ██ ██    ██ ██   ██ 
                          ██ ████ ██ ███████ ██ ██ ██  ██     ██      ██    ██ ██    ██ ██████  
                          ██  ██  ██ ██   ██ ██ ██  ██ ██     ██      ██    ██ ██    ██ ██      
                          ██      ██ ██   ██ ██ ██   ████     ███████  ██████   ██████  ██      
-------------------------------------------------------------------------------------------------------------------------*/                                                                        


void loop()
{
  audioHook();
  mqttHook();
  heartbeatHook();
  keepAliveHook();
  automataHook();
}
