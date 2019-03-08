#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include "DHT.h"
#include <ESP.h>

#define DHTTYPE DHT11   // DHT 22  (AM2302), AM2321

const char* WIFI_SSID = "YOUR_SSID_HERE";
const char* WIFI_PASSWORD = "YOUR_PASSWORD_HERE";

const int DEEPSLEEP_SECONDS = 180 * 1000 * 1000;

const int DHT_PIN = 22;
const int SOIL_PIN = 32;
const int POWER_PIN = 34;

const String OPENHAB_SOIL_MOISTURE_URL = "http://openhab/rest/items/SoilMoisture";
const String OPENHAB_TEMPERATURE_URL = "http://openhab/rest/items/Temperature";
const String OPENHAB_HUMIDITY_URL = "http://openhab/rest/items/Humidity";

DHT dht(DHT_PIN, DHTTYPE);

void setup() {
  dht.begin();
  
  Serial.begin(115200);
  while(!Serial) {
    ; 
  }
  esp_sleep_enable_timer_wakeup(DEEPSLEEP_SECONDS);

  pinMode(POWER_PIN, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  ArduinoOTA.setHostname("familyRoom-esp32");
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Started");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Ended");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  
  Serial.print("Posting values to endpoint\n");
  postToHttpEndpoint(OPENHAB_SOIL_MOISTURE_URL, String(getSoilMoisture()));
  delay(1000);
  postToHttpEndpoint(OPENHAB_TEMPERATURE_URL, String(getTemperature()));
  delay(1000);
  postToHttpEndpoint(OPENHAB_HUMIDITY_URL, String(getHumidity()));

  deepSleep();
}

void deepSleep() {
  esp_sleep_enable_timer_wakeup(DEEPSLEEP_SECONDS);
  esp_deep_sleep_start();
}

int getSoilMoisture() {
  int soilMoisture = analogRead(SOIL_PIN);
  soilMoisture = map(soilMoisture, 0, 4095, 0, 1023);
  soilMoisture = constrain(soilMoisture, 0, 1023);
  
  return soilMoisture;
}

float getTemperature() {
  return dht.readTemperature();
}

float getHumidity() {
  return dht.readHumidity();
}

float getHeatIndex(float temperature, float humidity) {
  return dht.computeHeatIndex(temperature, humidity, false);
}

void postToHttpEndpoint(String postURL, String postValue) {
  HTTPClient http;
  if (http.begin(postURL)) {
    Serial.print("HTTP POST\n");
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("Accept", "application/json");

    int httpCode = http.POST(postValue);
    
    if (httpCode > 0) {
      Serial.printf("HTTP POST response code: %d\n", httpCode);
    
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        Serial.print(payload);
        Serial.print("\n");
      }
    } else {
      Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.printf("HTTP Unable to connect\n");
  }
}
