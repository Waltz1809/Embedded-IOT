// === File: receiver.ino (Updated for Blynk IoT) ===

// THAY THẾ CÁC GIÁ TRỊ NÀY BẰNG THÔNG TIN TỪ BLYNK CONSOLE CỦA BẠN
#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"       // Ví dụ: TMPLxxxxxx
#define BLYNK_TEMPLATE_NAME "Testing"   // Ví dụ: My ESP32 Device
#define BLYNK_AUTH_TOKEN "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"   // Mã Auth Token vẫn cần thiết

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// --- Cấu hình WiFi (để Blynk kết nối) ---
char ssid[] = "Phuong Hoa";       // THAY THẾ BẰNG TÊN WIFI CỦA BẠN
char pass[] = "65dienbienphu";   // THAY THẾ BẰNG MẬT KHẨU WIFI CỦA BẠN
// -----------------------------------------

const int LED_PIN = 2;
const int WIFI_CHANNEL = 6; // PHẢI GIỐNG VỚI KÊNH TRÊN SENDER

char latestMsg[251];

// Callback function khi nhận được dữ liệu ESP-NOW
void onReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  // ... (phần còn lại của hàm onReceive giữ nguyên) ...
  memset(latestMsg, 0, sizeof(latestMsg));
  int copyLen = (len >= sizeof(latestMsg)) ? (sizeof(latestMsg) - 1) : len;
  memcpy(latestMsg, incomingData, copyLen);

  Serial.print("[RECV] Length: "); Serial.print(len);
  Serial.print(", Data: "); Serial.println(latestMsg);

  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, latestMsg);
    Blynk.logEvent("espnow_message", latestMsg);
  } else {
    Serial.println("Blynk not connected. Message not sent to cloud.");
  }

  digitalWrite(LED_PIN, HIGH);
  delay(50);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP-NOW Receiver Initializing (Blynk IoT)...");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  memset(latestMsg, 0, sizeof(latestMsg));

  // 1. Khởi tạo WiFi và đặt kênh
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("Setting WiFi channel to: ");
  Serial.println(WIFI_CHANNEL);
  esp_err_t channel_set_result = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channel_set_result != ESP_OK) {
    Serial.print("Error setting WiFi channel: ");
    Serial.println(esp_err_to_name(channel_set_result));
    Serial.println("Halting due to channel set error.");
    while (true) delay(1000);
  } else {
    Serial.println("WiFi channel set successfully.");
  }

  // 2. Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed!");
    while (true) delay(1000);
  }
  Serial.println("ESP-NOW initialized successfully.");

  // 3. Đăng ký callback nhận dữ liệu
  esp_err_t reg_status = esp_now_register_recv_cb(onReceive);
  if (reg_status != ESP_OK) {
    Serial.print("Failed to register ESP-NOW receive callback: ");
    Serial.println(esp_err_to_name(reg_status));
    while (true) delay(1000);
  }
  Serial.println("ESP-NOW receive callback registered.");

  // 4. Kết nối WiFi và Blynk
  Serial.print("Attempting to connect to WiFi SSID: ");
  Serial.println(ssid);
  
  // Với Blynk IoT, bạn thường chỉ cần gọi Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // Nó sẽ tự động xử lý kết nối WiFi.
  // Tuy nhiên, vì chúng ta muốn đặt kênh WiFi TRƯỚC KHI kết nối, chúng ta sẽ làm thủ công hơn một chút.
  
  WiFi.begin(ssid, pass); // Kết nối WiFi trên kênh đã đặt

  int connect_tries = 0;
  const int max_connect_tries = 20;
  while (WiFi.status() != WL_CONNECTED && connect_tries < max_connect_tries) {
    delay(500);
    Serial.print(".");
    connect_tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    Serial.print("WiFi Channel: "); Serial.println(WiFi.channel());

    // Bây giờ kết nối Blynk
    Blynk.config(BLYNK_AUTH_TOKEN); // Cấu hình Blynk với Auth Token
                                    // BLYNK_TEMPLATE_ID và BLYNK_TEMPLATE_NAME đã được define ở trên
    Blynk.connect();

    int blynk_connect_tries = 0;
    const int max_blynk_tries = 10;
    while(!Blynk.connected() && blynk_connect_tries < max_blynk_tries) {
        delay(500);
        Serial.print("B");
        Blynk.run(); // Gọi run để nó thử kết nối
        blynk_connect_tries++;
    }

    if (Blynk.connected()) {
        Serial.println("\nConnected to Blynk!");
    } else {
        Serial.println("\nFailed to connect to Blynk. ESP-NOW will still work.");
    }

  } else {
    Serial.println("\nFailed to connect to WiFi.");
    Serial.println("ESP-NOW will attempt to listen on the set channel, but Blynk will not be available.");
    Serial.print("Current WiFi Channel (may not be set channel if connection failed early): ");
    Serial.println(WiFi.channel());
  }

  Serial.println("Receiver ready.");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) { // Chỉ chạy Blynk.run() nếu có WiFi
    Blynk.run();
  }
  // Các tác vụ khác không phụ thuộc Blynk có thể đặt ở đây
}

