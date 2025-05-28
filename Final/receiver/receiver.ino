#include <esp_now.h>
#include <WiFi.h>
#define BLYNK_DEVICE_NAME   "Testing2"
#define BLYNK_AUTH_TOKEN    "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"
#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"

#include <BlynkSimpleEsp32.h>

char ssid[] = "Phuong Hoa";
char pass[] = "65dienbienphu";

char latestMsg[251]; // Bộ đệm lưu chuỗi mới nhận

// Callback CHUẨN cho ESP-NOW core mới (Arduino core 3.x+)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memset(latestMsg, 0, sizeof(latestMsg));
  strncpy(latestMsg, (const char*)incomingData, len > 250 ? 250 : len);
  Serial.print("Đã nhận từ Sender: ");
  Serial.println(latestMsg);

  // Đẩy lên Blynk: gửi vào Virtual Pin V0
  Blynk.virtualWrite(V0, latestMsg);

  // Nếu muốn gửi Notification trên app (nếu bạn đã bật logEvent cho Blynk Dashboard)
  Blynk.logEvent("espnow_message", latestMsg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Kết nối WiFi
  WiFi.begin(ssid, pass);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED && count < 30) {
    delay(500);
    Serial.print(".");
    count++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Kết nối WiFi thất bại!");
    while (1); // Treo lại để không chạy tiếp
  }
  Serial.println("\nWiFi OK");
  
  // Kết nối Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // ESP-NOW init
  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi khi khởi tạo ESP-NOW");
    return;
  }
  // Đăng ký callback CHUẨN mới
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  Blynk.run();
  // Khi nhận dữ liệu sẽ tự động đẩy lên Blynk qua callback
}
