// === File: receiver.ino ===
#include <esp_now.h>
#include <WiFi.h>
#define BLYNK_AUTH_TOKEN    "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"
#define BLYNK_TEMPLATE_ID   "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"
#include <BlynkSimpleEsp32.h>

char ssid[] = "Phuong Hoa";
char pass[] = "65dienbienphu";

unsigned long lastDataTime = 0;
int dataCount = 0;

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  dataCount++;
  lastDataTime = millis();
  
  Serial.println("\n=== ESP-NOW DATA RECEIVED ===");
  Serial.printf("Lần nhận #%d | Từ MAC: ", dataCount);
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.printf(" | Độ dài: %d bytes\n", len);
  
  if (len > 0) {
    char receivedData[251];
    memset(receivedData, 0, sizeof(receivedData));
    strncpy(receivedData, (const char*)data, len > 250 ? 250 : len);
    receivedData[len > 250 ? 250 : len] = '\0';
    
    Serial.print("Dữ liệu thô: ");
    Serial.println(receivedData);
    
    // Parse dữ liệu
    float values[7];
    int count = 0;
    char* token = strtok(receivedData, ",");
    while (token != NULL && count < 7) {
      values[count] = atof(token);
      count++;
      token = strtok(NULL, ",");
    }
    
    if (count >= 7) {
      // In ra Serial đơn giản
      Serial.print("✓ AccX:" + String(values[0], 1) + " AccY:" + String(values[1], 1) + " AccZ:" + String(values[2], 1));
      Serial.print(" | GyroX:" + String(values[3], 1) + " GyroY:" + String(values[4], 1) + " GyroZ:" + String(values[5], 1));
      Serial.print(" | Temp:" + String(values[6], 1) + "°C");
      
      // Gửi lên Blynk
      if (Blynk.connected()) {
        Blynk.virtualWrite(V0, receivedData);
        Blynk.virtualWrite(V1, values[6]); // Nhiệt độ
        Blynk.virtualWrite(V2, values[0]); // AccX
        Blynk.virtualWrite(V3, values[1]); // AccY
        Blynk.virtualWrite(V4, values[2]); // AccZ
        Serial.print(" | Blynk: ✓");
      } else {
        Serial.print(" | Blynk: ✗ (Mất kết nối)");
      }
      Serial.println();
    } else {
      Serial.printf("✗ Lỗi dữ liệu - chỉ parse được %d/%d giá trị\n", count, 7);
    }
  } else {
    Serial.println("✗ Nhận được dữ liệu rỗng");
  }
  Serial.println("=============================");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== ESP32 RECEIVER STARTING ===");
  
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.println("MAC Receiver: " + WiFi.macAddress());
  
  Serial.printf("Đang kết nối WiFi '%s'", ssid);
  WiFi.begin(ssid, pass);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi: ✓ Kết nối thành công");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi: ✗ Kết nối thất bại");
    return;
  }

  Serial.print("Đang kết nối Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println(Blynk.connected() ? " ✓" : " ✗");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW: ✗ Khởi tạo thất bại");
    return;
  }
  Serial.println("ESP-NOW: ✓ Khởi tạo thành công");
  
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("=== RECEIVER READY ===");
  Serial.println("Đang chờ dữ liệu từ sender...");
}

void loop() {
  Blynk.run();
  
  // Kiểm tra timeout (nếu không nhận data trong 10 giây)
  if (dataCount > 0 && millis() - lastDataTime > 10000) {
    Serial.println("⚠️  WARNING: Không nhận được dữ liệu trong 10 giây!");
    lastDataTime = millis(); // Reset để tránh spam
  }
  
  delay(100);
}
