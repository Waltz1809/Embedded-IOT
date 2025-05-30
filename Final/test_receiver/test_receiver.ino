// === File: receiver_debug.ino ===
#include <esp_now.h>
#include <WiFi.h>

char latestMsg[251];

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  Serial.println("\n[DEBUG] Đã vào OnDataRecv callback!");
  memset(latestMsg, 0, sizeof(latestMsg));
  strncpy(latestMsg, (const char*)data, len > 250 ? 250 : len);
  Serial.print("[ESP-NOW] Dữ liệu nhận: ");
  Serial.println(latestMsg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println("Khởi động WiFi STA");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW khởi động thất bại!");
    return;
  }

  Serial.println("ESP-NOW đã sẵn sàng");
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Không làm gì ngoài việc chờ dữ liệu ESP-NOW
  delay(10);
}
