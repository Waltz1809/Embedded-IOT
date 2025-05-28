#include <esp_now.h>
#include <WiFi.h>

// Thay đúng địa chỉ MAC của Receiver
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi ESP-NOW!");
    return;
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Lỗi add peer!");
    return;
  }
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); // Đọc tới khi gặp Enter
    input.trim(); // Loại bỏ khoảng trắng dư thừa
    if (input.length() > 0 && input.length() < 250) { // ESP-NOW max 250 bytes
      char buffer[251];
      input.toCharArray(buffer, 251); // Đảm bảo có ký tự '\0'
      esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)buffer, strlen(buffer)+1);
      if (result == ESP_OK) Serial.println("Gửi thành công");
      else Serial.println("Gửi thất bại");
    }
  }
}
