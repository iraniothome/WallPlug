#include <Arduino.h>

#include <FS.h>                   
#include <ESP8266WiFi.h>          
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         
#include <PubSubClient.h>        
#include <ArduinoJson.h>         
#include <WiFiUDP.h>
#include <ArduinoOTA.h>
#include <MQTT.h>
#include <EEPROM.h>
#include "ACS712.h"
#include <WiFiUDP.h>

#define DEBUG

WiFiUDP Udp;

float error=0.05f;
struct StoreStruct {
    bool state;     // state
  } 
  
storage = {
  
  0, // off
  
};

int buttonState;
boolean switchState;
const int buttonPin = 4;   //GPIO4
const int outPin = 2;      //GPIO2
int lastButtonState = LOW; 
int potenState;
const int potenPin = A0;   //ADC
int lastpotenState = 0;
ACS712 ac1(potenPin, error);
double current=0;
const int smartconfigled =  5;      // GPIO 5 the number of the LED pin   D1

// MQTT settings

WiFiClient espClient;

PubSubClient client(espClient);

long lastMsg = 0;

char msg[128];

long interval = 10000;     // interval at which to send mqtt messages (milliseconds)



//define your default values here, if there are different values in config.json, they are overwritten.

char mqtt_server[40] = "10.10.10.102";

char mqtt_port[6] = "1883";

char username[34] = "";

char password[34] = "";

const char* willTopic = "home/";

String node=String(ESP.getChipId());
const char* clientid=node.c_str();

const char* apName = clientid;
const char* apPass = clientid;



//flag for saving data

bool shouldSaveConfig = false;



//callback notifying us of the need to save config

void saveConfigCallback () {

  Serial.println("Should save config");

  shouldSaveConfig = true;

}



// Message received through MQTT

void callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");

  Serial.print(topic);

  Serial.print("] ");

  for (int i = 0; i < length; i++) {

    
    Serial.print("\nswitch state received: ");
    Serial.println((char)payload[i]);
  }

  Serial.println();



  // Switch on the relay if an 1 was received as first character on the home/ topic

  if (strcmp(topic, (String("home/wallplug")+clientid).c_str()) == 0){

    Serial.println("Topic recognized");

    if ((char)payload[0] == '1') {

    digitalWrite(outPin, HIGH);   // Turn the Relay on
    digitalWrite(BUILTIN_LED, switchState);
    switchState = !switchState; 
      //Serial.println(String("home/wallplug")+clientid+String(" : ")+String(switchState));
      //client.publish((String("home/wallplug")+clientid+String("/status")).c_str(),String(switchState).c_str());
      
     Serial.println(String("home/wallplug")+clientid+String("1"));
     client.publish((String("home/wallplug")+clientid+String("/status")).c_str(), "1");
     //client.subscribe((String("home/wallplug")+clientid+String("/status")).c_str());

    } 
  else  if ((char)payload[0]=='0') {
    
    digitalWrite(outPin, LOW);   // Turn the Relay off
     digitalWrite(BUILTIN_LED, switchState);
    switchState = !switchState; 
      //Serial.println(String("home/wallplug")+clientid+String(" : ")+String(!switchState));
      //client.publish((String("home/wallplug")+clientid+String("/status")).c_str(),String(!switchState).c_str());
   
      Serial.println(String("home/wallplug")+clientid+String("0"));
      client.publish((String("home/wallplug")+clientid+String("/status")).c_str(), "0");
      //client.subscribe((String("home/wallplug")+clientid+String("/status")).c_str());
    }


  }



}

void setup() {
  //ArduinoOTA.setPassword((const char *)"7376");
  ArduinoOTA.begin();

  pinMode(outPin, OUTPUT);     // Initialize the outPin as an output

  Serial.begin(115200);

  Serial.println();

  int cnt = 0;
  

   pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, HIGH);
  


  switchState = LOW;  


  
  pinMode(BUILTIN_LED, OUTPUT); 
  digitalWrite(BUILTIN_LED, !switchState); 
#ifndef DEBUG
  pinMode(outPin, OUTPUT); 
  digitalWrite(outPin, switchState);
#endif  

// initialize the LED pin as an output:
  pinMode(smartconfigled, OUTPUT);
  
  
WiFi.mode(WIFI_STA);
digitalWrite(smartconfigled, HIGH);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print (".");
    if (cnt++ >= 10) {
      WiFi.beginSmartConfig();
      while (1) {
        delay (1000);
        if (WiFi.smartConfigDone()) {
          Serial.println("Smart Config done");
          
          break;
        }
      }
    }
  }

  //clean FS, for testing

  //SPIFFS.format();



  //read configuration from FS json

  Serial.println("mounting FS...");



  if (SPIFFS.begin()) {

    Serial.println("mounted file system");

    if (SPIFFS.exists("/config.json")) {

      //file exists, reading and loading

      Serial.println("reading config file");

      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile) {

        Serial.println("opened config file");

        size_t size = configFile.size();

        // Allocate a buffer to store contents of the file.

        std::unique_ptr<char[]> buf(new char[size]);



        configFile.readBytes(buf.get(), size);

        DynamicJsonBuffer jsonBuffer;

        JsonObject& json = jsonBuffer.parseObject(buf.get());

        json.printTo(Serial);

        if (json.success()) {

          Serial.println("\nparsed json");



          strcpy(mqtt_server, json["mqtt_server"]);

          strcpy(mqtt_port, json["mqtt_port"]);

          strcpy(username, json["username"]);

          strcpy(password, json["password"]);



        } else {

          Serial.println("failed to load json config");

        }

      }

    }

  } else {

    Serial.println("failed to mount FS");

  }
  

  //save the custom parameters to FS

  if (shouldSaveConfig) {

    Serial.println("saving config");

    DynamicJsonBuffer jsonBuffer;

    JsonObject& json = jsonBuffer.createObject();

    json["mqtt_server"] = mqtt_server;

    json["mqtt_port"] = mqtt_port;

    json["username"] = username;

    json["password"] = password;



    File configFile = SPIFFS.open("/config.json", "w");

    if (!configFile) {

      Serial.println("failed to open config file for writing");

    }



    json.printTo(Serial);

    json.printTo(configFile);

    configFile.close();

    //end save

  }

          Serial.println("local ip");
          Serial.println(WiFi.localIP());
          Serial.println(clientid);
          
  // mqtt

  client.setServer(mqtt_server, atoi(mqtt_port)); // parseInt to the port

  client.setCallback(callback);



}





void reconnect() {

  // Loop until we're reconnected to the MQTT server

  while (!client.connected()) {

    Serial.print("Attempting MQTT connection...");

    // Attempt to connect

    if (client.connect(username, username, password, willTopic, 0, 1, (String("I'm dead: ")+username).c_str())) {     // username as client ID

      Serial.println("connected");
      digitalWrite(smartconfigled, LOW);
      // Once connected, publish an announcement... (if not authorized to publish the connection is closed)

      client.publish("all", (String("home/wallplug")+clientid).c_str());

      // ... and resubscribe

      client.subscribe((String("home/wallplug")+clientid).c_str());


      delay(5000);

      

    } else {

      Serial.print("failed, rc=");

      Serial.print(client.state());

      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying

      delay(5000);

    }

  }

}

void potentiometer(){
  float current = ac1.getACcurrent();

 //Serial.print(current);

 //Serial.println(" Amps RMS");
 
if( current >= 0.0 && current < 0.05){
  Serial.println(String("home/wallplug")+clientid+String("almost No Use"));
  client.publish((String("home/wallplug")+clientid+String("/status")).c_str(), "almost No Use");
}
if (current >= 0.5 && current <= 3.0){
  Serial.println(String("home/wallplug")+clientid+String(" Normal Use"));
  client.publish((String("home/wallplug")+clientid+String("/status")).c_str(), "Normal Use");
  //client.publish((String("home/wallplug")+clientid+String("/current")).c_str(), current, true); // retained message
  }

else if ( current > 3.0 && current <= 4.0){
  Serial.println(String("home/wallplug")+clientid+String("Warning"));
  client.publish((String("home/wallplug")+clientid+String("/status")).c_str(), "Warning");
}
else if ( current > 4.0){
  Serial.println(String("home/wallplug")+clientid+String("Alert"));
  client.publish((String("home/wallplug")+clientid+String("/status")).c_str(), "Alert");
}


}



void loop() {

int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    if (reading == LOW)
    {
      switchState = !switchState;            
#ifdef DEBUG
      Serial.println("button pressed");
      digitalWrite(outPin, !switchState);   // Turn the Relay on
      Serial.println(String("home/wallplug")+clientid+String(" : ")+String(!switchState));
      client.publish((String("home/wallplug")+clientid+String("/status")).c_str(),String(!switchState).c_str());
      client.subscribe((String("home/wallplug")+clientid+String("/status")).c_str());
#endif
    }

    lastButtonState = reading;
  }
  
  digitalWrite(BUILTIN_LED, !switchState); 
#ifndef DEBUG  
  digitalWrite(outPin, !switchState); 
#endif
  if (switchState != storage.state)
  {
    storage.state = !switchState;

  }




  if (!client.connected()) { // MQTT

    reconnect();

  }




  client.loop();



  unsigned long now = millis();

 

  if(now - lastMsg > interval) {

    // save the last time you blinked the LED 

    lastMsg = now;

    potentiometer();
    

     //Serial.println(String("home/relay")+clientid+String("/status"));
     //Serial.print(client.state());


  }

 
 ArduinoOTA.handle(); 

}



