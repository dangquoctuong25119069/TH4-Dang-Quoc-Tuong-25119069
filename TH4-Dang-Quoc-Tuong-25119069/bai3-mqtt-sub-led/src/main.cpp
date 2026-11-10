#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Adafruit_SHT31 sht31;
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);

String topicTele;
String topicCmd;
String topicStatus;
String clientId;

bool shtReady = false;
bool oledReady = false;

bool ledOn = false;
bool buttonPressed = false;
uint32_t pressCount = 0;
const char* ledSource = "boot";
bool sensorOk = false;
float lastTemp = NAN, lastHum = NAN;

bool btnLastRaw = HIGH;
bool btnStable = HIGH;
unsigned long btnChangedAt = 0;

unsigned long lastPublish = 0, lastSensorRead = 0, lastOled = 0;
unsigned long lastWifiTry = 0, lastMqttTry = 0;
bool wifiWasConnected = false;
bool publishNow = false;
bool oledDirty = true;
uint32_t seq = 0;

float round1(float x) { return roundf(x * 10.0f) / 10.0f; }

void setLed(bool on, const char* source) {
  ledOn = on;
  ledSource = source;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
  publishNow = true;
  oledDirty = true;
  Serial.printf("LED -> %s (%s)\n", on ? "BAT" : "TAT", source);
}

void readSensor() {
#if SENSOR_MODE == SENSOR_SIM
  float t = millis() / 1000.0f;
  lastTemp = 27.0f + 1.5f * sinf(t / 30.0f);
  lastHum  = 60.0f + 5.0f * sinf(t / 45.0f);
  sensorOk = true;
#else
  if (!shtReady) shtReady = sht31.begin(SHT31_ADDR);
  if (shtReady) {
    lastTemp = sht31.readTemperature();
    lastHum  = sht31.readHumidity();
  }
  sensorOk = shtReady && !isnan(lastTemp) && !isnan(lastHum);
  if (!sensorOk) { lastTemp = NAN; lastHum = NAN; }
#endif
  oledDirty = true;
}

void readButton() {
  bool raw = digitalRead(PIN_BUTTON);
  if (raw != btnLastRaw) {
    btnLastRaw = raw;
    btnChangedAt = millis();
  }
  if (millis() - btnChangedAt >= DEBOUNCE_MS && raw != btnStable) {
    btnStable = raw;
    buttonPressed = (btnStable == LOW);
    if (buttonPressed) {
      pressCount++;
      setLed(!ledOn, "button");
    }
    publishNow = true;
    oledDirty = true;
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (topicCmd != topic) return;

  if (length == 1 && payload[0] == '1') {
    setLed(true, "dashboard");
  } else if (length == 1 && payload[0] == '0') {
    setLed(false, "dashboard");
  } else {
    Serial.printf("Bo qua lenh sai dinh dang tren %s (len=%u)\n", topic, length);
  }
}

void publishTelemetry() {
  JsonDocument doc;
  doc["id"] = SV_ID;
  if (sensorOk) {
    doc["temp"] = round1(lastTemp);
    doc["hum"]  = round1(lastHum);
  } else {
    doc["temp"] = nullptr;
    doc["hum"]  = nullptr;
    doc["error"] = "sensor_read_failed";
  }
  doc["uptime"]  = millis() / 1000;
  doc["rssi"]    = WiFi.RSSI();
  doc["led"]     = ledOn;
  doc["button"]  = buttonPressed;
  doc["presses"] = pressCount;
  doc["led_src"] = ledSource;
  doc["seq"]     = seq++;
  doc["sensor"]  = SENSOR_MODE == SENSOR_SIM ? "sim" : "sht31";

  char buf[384];
  size_t len = serializeJson(doc, buf, sizeof(buf));
  if (len == 0 || len >= sizeof(buf) - 1) {
    Serial.println("Loi: JSON vuot qua buffer, khong gui");
    return;
  }
  bool sent = mqtt.publish(topicTele.c_str(), buf);
  Serial.printf("%s %s\n", sent ? "PUB" : "PUB LOI", buf);
}

void checkWifi() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifiWasConnected) {
    if (connected) {
      Serial.printf("Wi-Fi OK, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("Mat Wi-Fi");
    }
    wifiWasConnected = connected;
    oledDirty = true;
  }
  if (!connected && millis() - lastWifiTry >= WIFI_RETRY_MS) {
    lastWifiTry = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void checkMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (millis() - lastMqttTry < MQTT_RETRY_MS) return;
  lastMqttTry = millis();

  Serial.printf("Ket noi MQTT %s:%d... ", MQTT_HOST, MQTT_PORT);
  if (mqtt.connect(clientId.c_str(), topicStatus.c_str(), 1, true, "offline")) {
    Serial.println("OK");
    mqtt.publish(topicStatus.c_str(), "online", true);

    mqtt.subscribe(topicCmd.c_str(), 1);
    Serial.printf("Subscribe %s\n", topicCmd.c_str());
    publishNow = true;
  } else {
    Serial.printf("loi, state=%d\n", mqtt.state());
  }
  oledDirty = true;
}

void drawOled() {
  if (!oledReady) return;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.printf("TH4 %s", SV_ID);

  oled.setCursor(0, 12);
  if (sensorOk) {
    oled.printf("T:%.1fC  H:%.1f%%", lastTemp, lastHum);
  } else {
    oled.print("Loi cam bien");
  }

  oled.setTextSize(2);
  oled.setCursor(0, 24);
  oled.print(ledOn ? "LED: BAT" : "LED: TAT");

  oled.setTextSize(1);
  oled.setCursor(0, 44);
  oled.printf("Nut:%s n=%lu %s", buttonPressed ? "NHAN" : "tha ",
              (unsigned long)pressCount, ledSource[0] == 'd' ? "<web" : "");

  oled.setCursor(0, 56);
  oled.printf("WiFi:%s MQTT:%s",
              WiFi.status() == WL_CONNECTED ? "OK" : "--",
              mqtt.connected() ? "OK" : "--");
  oled.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  topicTele   = String(TOPIC_ROOT) + "/" + SV_ID + "/telemetry";
  topicCmd    = String(TOPIC_ROOT) + "/" + SV_ID + "/cmd/led";
  topicStatus = String(TOPIC_ROOT) + "/" + SV_ID + "/status";
  char chip[13];
  snprintf(chip, sizeof(chip), "%012llx", ESP.getEfuseMac());
  clientId = String("aet2022-") + SV_ID + "-" + chip;

  Serial.printf("\n== AET2022 TH4 Bai 3 | %s | %s ==\n", STUDENT_NAME, SV_ID);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledReady = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.println(oledReady ? "OLED OK" : "Khong thay OLED (kiem tra dia chi 0x3C/0x3D)");
#if SENSOR_MODE == SENSOR_SHT31
  shtReady = sht31.begin(SHT31_ADDR);
  Serial.println(shtReady ? "SHT31 OK" : "Khong thay SHT31");
#else
  Serial.println("CHE DO MO PHONG: so do khong phai tu cam bien that");
#endif
  readSensor();
  drawOled();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiTry = millis();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(5);
  mqtt.setKeepAlive(15);
}

void loop() {

  readButton();

  if (millis() - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = millis();
    readSensor();
  }

  checkWifi();
  checkMqtt();
  mqtt.loop();

  if (mqtt.connected()) {
    if (publishNow) {
      publishNow = false;
      publishTelemetry();
    } else if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
      lastPublish = millis();
      publishTelemetry();
    }
  }

  if (oledDirty && millis() - lastOled >= OLED_INTERVAL_MS) {
    lastOled = millis();
    oledDirty = false;
    drawOled();
  }
}
