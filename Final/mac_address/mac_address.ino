#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);               // Chuyển Wi-Fi sang chế độ Station
  Serial.println(WiFi.macAddress()); // In địa chỉ MAC của ESP32 này ra Serial
}

void loop() {
  // Không cần nội dung gì trong loop
}
