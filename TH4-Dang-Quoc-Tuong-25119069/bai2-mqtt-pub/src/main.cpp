#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include "config.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Adafruit_SHT31 sht31;
bool shtReady = false;

String topicTele;
String topicStatus;
String clientId;

unsigned long lastPublish = 0;
unsigned long lastWifiTry = 0;
unsigned long lastMqttTry = 0;
bool wifiWasConnected = false;
uint32_t seq = 0;

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

float round1(float x) { return roundf(x * 10.0f) / 10.0f; }

void checkWifi() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wifiWasConnected) {
    Serial.printf("Wi-Fi OK, IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (!connected && wifiWasConnected) {
    Serial.println("Mat Wi-Fi");
  }
  wifiWasConnected = connected;

  if (!connected && millis() - lastWifiTry >= WIFI_RETRY_MS) {
    lastWifiTry = millis();
    Serial.printf("Ket noi Wi-Fi \"%s\"...\n", WIFI_SSID);
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void checkMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (millis() - lastMqttTry < MQTT_RETRY_MS) return;
  lastMqttTry = millis();

  Serial.printf("Ket noi MQTT %s:%d (client id %s)... ", MQTT_HOST, MQTT_PORT, clientId.c_str());
  bool ok = mqtt.connect(clientId.c_str(), topicStatus.c_str(), 1, true, "offline");
  if (ok) {
    Serial.println("OK");
    mqtt.publish(topicStatus.c_str(), "online", true);
  } else {
    Serial.printf("loi, state=%d\n", mqtt.state());
  }
}

void publishTelemetry() {
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
  doc["seq"]    = seq++;
  doc["sensor"] = SENSOR_MODE == SENSOR_SIM ? "sim" : "sht31";

  char buf[256];
  size_t len = serializeJson(doc, buf, sizeof(buf));
  if (len == 0 || len >= sizeof(buf) - 1) {
    Serial.println("Loi: JSON vuot qua buffer, khong gui");
    return;
  }
  bool sent = mqtt.publish(topicTele.c_str(), buf);
  Serial.printf("%s %s %s\n", sent ? "PUB" : "PUB LOI", topicTele.c_str(), buf);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  topicTele   = String(TOPIC_ROOT) + "/" + SV_ID + "/telemetry";
  topicStatus = String(TOPIC_ROOT) + "/" + SV_ID + "/status";
  char chip[13];
  snprintf(chip, sizeof(chip), "%012llx", ESP.getEfuseMac());
  clientId = String("aet2022-") + SV_ID + "-" + chip;

  Serial.printf("\n== AET2022 TH4 Bai 2 MQTT pub | %s | %s ==\n", STUDENT_NAME, SV_ID);
  Serial.printf("Topic: %s\n", topicTele.c_str());

#if SENSOR_MODE == SENSOR_SHT31
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  shtReady = sht31.begin(SHT31_ADDR);
  Serial.println(shtReady ? "SHT31 OK" : "Khong thay SHT31 (kiem tra day I2C / dia chi)");
#else
  Serial.println("CHE DO MO PHONG: so do khong phai tu cam bien that");
#endif

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiTry = millis();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(5);
  mqtt.setKeepAlive(15);
}

void loop() {
  checkWifi();
  checkMqtt();
  mqtt.loop();

  if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = millis();
    if (mqtt.connected()) publishTelemetry();
  }
}
