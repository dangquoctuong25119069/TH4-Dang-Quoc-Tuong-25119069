#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include "config.h"

WebServer server(80);
Adafruit_SHT31 sht31;
bool shtReady = false;
unsigned long lastWifiCheck = 0;

struct SensorData {
  bool ok;
  float temp;
  float hum;
};

SensorData readSensor() {
  SensorData d = {false, NAN, NAN};
#if SENSOR_MODE == SENSOR_SIM
  float t = millis() / 1000.0f;
  d.temp = 27.0f + 1.5f * sinf(t / 30.0f);
  d.hum  = 60.0f + 5.0f * sinf(t / 45.0f);
  d.ok = true;
#else
  if (!shtReady) {
    shtReady = sht31.begin(SHT31_ADDR);
    if (!shtReady) return d;
  }
  d.temp = sht31.readTemperature();
  d.hum  = sht31.readHumidity();
  d.ok = !isnan(d.temp) && !isnan(d.hum);
#endif
  return d;
}

const char* sensorName() {
  return SENSOR_MODE == SENSOR_SIM ? "sim" : "sht31";
}

float round1(float x) { return roundf(x * 10.0f) / 10.0f; }

void handleSensor() {
  SensorData d = readSensor();

  JsonDocument doc;
  doc["id"] = SV_ID;
  if (d.ok) {
    doc["temp"] = round1(d.temp);
    doc["hum"]  = round1(d.hum);
  } else {
    doc["temp"] = nullptr;
    doc["hum"]  = nullptr;
    doc["error"] = "sensor_read_failed";
  }
  doc["uptime"] = millis() / 1000;
  doc["rssi"]   = WiFi.RSSI();
  doc["sensor"] = sensorName();

  String out;
  serializeJson(doc, out);
  server.send(d.ok ? 200 : 503, "application/json", out);
  Serial.printf("GET /api/sensor -> %d %s\n", d.ok ? 200 : 503, out.c_str());
}

void handleRoot() {
  server.send(200, "text/plain", "AET2022 TH4 - Bai 1. Endpoint: GET /api/sensor\n");
}

void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"not_found\"}");
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Dang ket noi Wi-Fi \"%s\"", WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi OK, IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Serial.printf("Mo: http://%s/api/sensor\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Chua vao duoc Wi-Fi, se thu lai trong loop()");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n== AET2022 TH4 Bai 1 REST | %s | %s ==\n", STUDENT_NAME, SV_ID);

#if SENSOR_MODE == SENSOR_SHT31
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  shtReady = sht31.begin(SHT31_ADDR);
  Serial.println(shtReady ? "SHT31 OK" : "Khong thay SHT31 (kiem tra day I2C / dia chi)");
#else
  Serial.println("CHE DO MO PHONG: so do khong phai tu cam bien that");
#endif

  connectWifi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/sensor", HTTP_GET, handleSensor);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("WebServer cong 80 da chay");
}

void loop() {
  server.handleClient();

  if (millis() - lastWifiCheck >= WIFI_RETRY_MS) {
    lastWifiCheck = millis();
    static bool wasConnected = false;
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected && !wasConnected) {
      Serial.printf("Wi-Fi OK, IP: %s -> http://%s/api/sensor\n",
                    WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
    } else if (!connected) {
      Serial.println("Mat Wi-Fi, dang ket noi lai...");
      WiFi.reconnect();
    }
    wasConnected = connected;
  }
}
