// === File: sender.ino ===
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 25
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_MPU6050 mpu;
float accX, accY, accZ;
float temperature = 0.0;

uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100); // Ổn định trước khi init ESP-NOW

  Wire.begin(27, 26);
  sensors.begin();
  delay(100);

  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU init fail");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAIL (code 12393)");
    while (true); // Treo tại đây để tránh gửi lỗi
  }
  Serial.println("ESP-NOW init OK");

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (!esp_now_is_peer_exist(receiverMAC)) {
    esp_err_t peerAdd = esp_now_add_peer(&peerInfo);
    if (peerAdd != ESP_OK) {
      Serial.print("Add peer failed: ");
      Serial.println(peerAdd);
    } else {
      Serial.println("Peer added OK");
    }
  }
}

void loop() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  accX = a.acceleration.x;
  accY = a.acceleration.y;
  accZ = a.acceleration.z;

  char msg[100];
  snprintf(msg, sizeof(msg), "T:%.2f X:%.2f Y:%.2f Z:%.2f", temperature, accX, accY, accZ);

  esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)msg, strlen(msg));
  if (result == ESP_OK) {
    Serial.print("OK: "); Serial.println(msg);
  } else {
    Serial.print("FAIL: "); Serial.println(result);
  }

  delay(3000);
}
