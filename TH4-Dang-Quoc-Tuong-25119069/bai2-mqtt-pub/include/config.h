#pragma once

#define STUDENT_NAME "Dang Quoc Tuong"
#define SV_ID        "25119069"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
#define TOPIC_ROOT "aet2022"

#define PUBLISH_INTERVAL_MS 2000
#define WIFI_RETRY_MS       10000
#define MQTT_RETRY_MS       5000

#define SENSOR_SHT31 1
#define SENSOR_SIM   2
#define SENSOR_MODE  SENSOR_SHT31

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define SHT31_ADDR  0x44
