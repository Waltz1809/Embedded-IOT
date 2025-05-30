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

void onReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memset(latestMsg, 0, sizeof(latestMsg));
  strncpy(latestMsg, (const char*)incomingData, len > 250 ? 250 : len);

  Serial.print("[RECV] ");
  Serial.println(latestMsg);

  Blynk.virtualWrite(V0, latestMsg);
  Blynk.logEvent("espnow_message", latestMsg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 20) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi FAIL");
    while (true);
  }
  Serial.println("\nWiFi OK");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true);
  }
  Serial.println("ESP-NOW receiver ready");

  esp_now_register_recv_cb(onReceive);
}

void loop() {
  Blynk.run();
}
