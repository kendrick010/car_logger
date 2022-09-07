// WiFi
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "credentials.hpp"

// SD-Card
#include <SPI.h>
#include <SD.h>

// GPS
#include "getGPS.hpp"

// ThingSpeak
#include "ThingSpeak.h"

#define WIFI_TIMEOUT_MS 20000
#define THINGSPEAK_TIMEOUT_MS 30000
#define WATCHDOG_TIMEOUT_S 30

WiFiClient client;
File file;

// Save last ThingSpeak request
unsigned long lastMS; 

// connectWifiTask: Maintain wifi connection
void connectWifiTask (void * parameters) {
  esp_task_wdt_init(30, false);
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidPhone, passwordPhone);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) { }

    if (WiFi.status() != WL_CONNECTED) { vTaskDelay(20000 / portTICK_PERIOD_MS); }
  }
}

// Log each field to ThingSpeak
void writeToThingSpeak() {
  ThingSpeak.begin(client);

  ThingSpeak.setField(1, latitude);
  ThingSpeak.setField(2, longitude);
  ThingSpeak.setField(3, timeRecord);
  ThingSpeak.setField(4, date);
  ThingSpeak.setField(5, speed);

  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
}

// Log each field to SD-Card
void writeToSD() {
  file = SD.open("/datalog.txt", FILE_APPEND);
  if (file) {
    String dataGPS = latitude + "|" + longitude + "|" + date + "|" + timeRecord + "|" + speed;
    file.println(dataGPS);
    Serial.println("----------- Appended to datalog.txt -----------");
    Serial.println("\n");
  } else {
    Serial.println("----------- Error opening datalog.txt -----------");
    Serial.println("\n");
  }
  file.close();
}

void setup() {
  Serial.begin(115200);
  ss.begin(9600);

  if (!SD.begin()) {
    Serial.println("SD-Card initialization failed");
    while (1);
  }
  Serial.println("SD-Card initialization done");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);

  /*Syntax for assigning task to a core:
  xTaskCreatePinnedToCore(
                  coreTask,       // Function to implement the task
                  "coreTask",     // Name of the task 
                  10000,          // Stack size in words 
                  NULL,           // Task input parameter 
                  0,              // Priority of the task 
                  NULL,           // Task handle. 
                  taskCore);      // Core where the task should run 
  */
  
  xTaskCreatePinnedToCore(connectWifiTask, "connectWifiTask", 5000, NULL, 1, NULL, 0);        
  delay(500); 
}

void loop() {
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      getLocation();
      getDate();
      getTime();
      getSpeed();
      // All invalid gps data will still store previous valid data
      if (WiFi.status() == WL_CONNECTED) { Serial.println("Connected to internet"); }
      else { Serial.println("Disconnected"); }
      printGPSData();
    }
  }

  if (millis() - lastMS >= THINGSPEAK_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) { writeToThingSpeak(); }
    else { writeToSD(); }
    lastMS = millis();  // Get ready for the next iteration
  }
}