// === File: sender.ino ===
#include <esp_now.h>
#include <WiFi.h>
#include "mpu6050_module.h"

MPU6050Module mpuSensor;
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC}; // Đổi theo MAC receiver thực tế

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (!mpuSensor.begin()) {
    Serial.println("MPU6050 không khởi động được!");
    while (1);
  }
  Serial.println("MPU6050 OK");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_add_peer(&peerInfo)) {
    Serial.println("Thêm receiver OK");
  }
}

void loop() {
  mpuSensor.update();
  String data = mpuSensor.serialize();
  esp_now_send(receiverMAC, (uint8_t*)data.c_str(), data.length() + 1);
  Serial.println("Đã gửi: " + data);
  delay(1000);
}

// === File: receiver.ino ===
#include <esp_now.h>
#include <WiFi.h>
#define BLYNK_AUTH_TOKEN    "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"
#define BLYNK_TEMPLATE_ID   "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"
#include <BlynkSimpleEsp32.h>

char ssid[] = "Phuong Hoa";
char pass[] = "65dienbienphu";

char latestMsg[251];

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  memset(latestMsg, 0, sizeof(latestMsg));
  strncpy(latestMsg, (const char*)data, len > 250 ? 250 : len);
  Serial.print("Dữ liệu MPU6050: ");
  Serial.println(latestMsg);
  Blynk.virtualWrite(V0, latestMsg);
  Blynk.logEvent("mpu6050_data", latestMsg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  Blynk.run();
}