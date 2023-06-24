#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <espnow.h>
#include "DHT.h"

#define DHTTYPE DHT11

#define dht_pin 0
DHT dht(dht_pin, DHTTYPE); 

#define ssid "Wednesday"
#define password "anddda26"

// Timer = 20 secunde
unsigned long timerDelay = 20000;
unsigned long lastTime = 0;

// Analog Output of battery
int analogValue;          
float calibration = 0.22;

// RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x48, 0x3F, 0xDA, 0x5D, 0x70, 0x76};

struct Station{
  double temp;
  int humid;
  int rain;
  double batteryVoltage; 
} stationData;


//Structura mesajului transmis prin ESP-NOW
//Trebuie sa fie identica cu cea a receiverului
struct Message {
  unsigned int readingId; 
  double temp;
  double humid;
  int rain;
  double batteryVoltage; 
  
} structMessage;


unsigned int readingId = 0;


void readTemp()
{
  stationData.temp = dht.readTemperature();
}

void readHmd()
{
  stationData.humid = dht.readHumidity();  
}

void readRain()
{
  int data = digitalRead(5);
  if(data == 0)
  {
    stationData.rain = 100;
  }
  else if(data == 1)
  {
    stationData.rain = 0;
  }
}

void readBattery()
{
  analogValue = analogRead(A0);
  stationData.batteryVoltage = (((analogValue * 3.3) / 1024) * 2 + calibration); 
}

void getStationData(){
  readTemp();
  readHmd();
  readRain();
  readBattery();
}

// callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}

void setup() {
 // Initializare interfata seriala
  Serial.begin(115200);

  //initializare DHT
  dht.begin();
 
  // Setam device-ul ca Station si Soft Access Point
  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Waiting for WI-FI..");
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  // Initializare ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //Setare rol MASTER
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  // Inregistrare callback
  esp_now_register_send_cb(OnDataSent);
  
  // Inregistrare pereche
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}


void setMessageParameters(){
 
  structMessage.temp = stationData.temp;
  structMessage.humid = stationData.humid;
  structMessage.rain = stationData.rain;

  structMessage.batteryVoltage = stationData.batteryVoltage;

  structMessage.readingId = readingId++;
  
  Serial.print("ID: ");
  Serial.println(structMessage.readingId);
  
  Serial.print("---TEMP: ");
  Serial.println(structMessage.temp);
  Serial.print("---HUMID: ");
  Serial.println(structMessage.humid);
  Serial.print("---RAIN: ");
  Serial.println(structMessage.rain);
  Serial.print("---BATTERY: ");
  Serial.println(structMessage.batteryVoltage);
  
}

void loop() {

  if ((millis() - lastTime) > timerDelay) {
    getStationData();
    
    if(WiFi.status()== WL_CONNECTED){        
         setMessageParameters();
      
         //Send message via ESP-NOW
         esp_now_send(broadcastAddress, (uint8_t *) &structMessage, sizeof(structMessage));
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    
    lastTime = millis();
  }
}
