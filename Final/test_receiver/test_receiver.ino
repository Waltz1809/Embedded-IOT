#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"
#define BLYNK_AUTH_TOKEN "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"

#define BLYNK_PRINT Serial // Cho phép Blynk in debug ra Serial Monitor
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h> // Cho hàm max() nếu cần

//=============== CẤU HÌNH WIFI VÀ ESP-NOW ===============
char ssid[] = "Phuong Hoa";                 // <<<<====== THAY TÊN WIFI CỦA BẠN
char pass[] = "65dienbienphu";             // <<<<====== THAY MẬT KHẨU WIFI CỦA BẠN
const int WIFI_CHANNEL = 11;                    // Phải giống với Sender

//=============== THIẾT BỊ CẢNH BÁO CỤC BỘ TẠI RECEIVER ===============
#define RECEIVER_ALARM_LED_PIN 16
#define RECEIVER_ALARM_BUZZER_PIN 17

//=============== CẤU TRÚC DỮ LIỆU (PHẢI GIỐNG HỆT SENDER) ===============
struct SensorData {
  float temp_drill;
  float temp_center;
  float temp_mine;
  float vibration;
  int gas_mq2;
  int gas_mq5;
  int gas_mq9;
  int gas_mq135;
};

//=============== NGƯỠNG CẢNH BÁO TẠI RECEIVER VÀ BLYNK ===============
const struct ReceiverAlertThresholds {
  float temp_drill_critical = 70.0;
  float temp_center_critical = 40.0;
  float temp_mine_critical = 40.0;
  float vibration_severe = 18.0;
  int gas_severe = 1500;
} RCV_THRESHOLDS;

// Biến cho Blynk notification cooldown
bool blynkAlertActive = false;
unsigned long lastBlynkAlertTime = 0;
const unsigned long BLYNK_NOTIFICATION_COOLDOWN = 10000; // 10 giây

//=============== VIRTUAL PINS CHO BLYNK ===============
// Đặt tên VPIN rõ ràng
#define VPIN_TEMP_DRILL    V1
#define VPIN_TEMP_CENTER   V2
#define VPIN_TEMP_MINE     V3
#define VPIN_VIBRATION     V4
#define VPIN_GAS_MQ2       V5
#define VPIN_GAS_MQ5       V6
#define VPIN_GAS_MQ9       V7
#define VPIN_GAS_MQ135     V8
#define VPIN_TERMINAL_MSG  V0 // Terminal hoặc widget hiển thị thông báo

//=============== XỬ LÝ CẢNH BÁO TẠI RECEIVER VÀ GỬI LÊN BLYNK ===============
void handleReceiverAlertsAndBlynk(const SensorData &data) {
  bool localAlertState = false;
  String alertMessage = ""; // Bắt đầu trống, chỉ thêm khi có cảnh báo

  if (data.temp_drill >= RCV_THRESHOLDS.temp_drill_critical) {
    alertMessage += "Mũi khoan nóng (" + String(data.temp_drill, 1) + "C)! ";
    localAlertState = true;
  }
  if (data.temp_center >= RCV_THRESHOLDS.temp_center_critical) {
    alertMessage += "Trung tâm nóng (" + String(data.temp_center, 1) + "C)! ";
    localAlertState = true;
  }
  if (data.temp_mine >= RCV_THRESHOLDS.temp_mine_critical) {
    alertMessage += "Khu mỏ nóng (" + String(data.temp_mine, 1) + "C)! ";
    localAlertState = true;
  }
  if (data.vibration >= RCV_THRESHOLDS.vibration_severe) {
    alertMessage += "Rung mạnh (" + String(data.vibration, 1) + ")! ";
    localAlertState = true;
  }
  // Kiểm tra chung cho các cảm biến khí
  if (data.gas_mq2 >= RCV_THRESHOLDS.gas_severe ||
      data.gas_mq5 >= RCV_THRESHOLDS.gas_severe ||
      data.gas_mq9 >= RCV_THRESHOLDS.gas_severe ||
      data.gas_mq135 >= RCV_THRESHOLDS.gas_severe) {
    alertMessage += "Nguy cơ khí độc! ";
    localAlertState = true;
  }

  // Kích hoạt cảnh báo cục bộ tại Receiver
  digitalWrite(RECEIVER_ALARM_LED_PIN, localAlertState);
  digitalWrite(RECEIVER_ALARM_BUZZER_PIN, localAlertState);

  // Gửi thông báo đến Blynk (với cooldown)
  unsigned long currentTime = millis();
  if (localAlertState) {
    if (!blynkAlertActive || (currentTime - lastBlynkAlertTime > BLYNK_NOTIFICATION_COOLDOWN)) {
      Serial.println("BLYNK ALERT: " + alertMessage);
      Blynk.logEvent("critical_alert", alertMessage.substring(0, min((int)alertMessage.length(), 250))); // Giới hạn độ dài
      Blynk.virtualWrite(VPIN_TERMINAL_MSG, alertMessage); // Cũng gửi lên terminal/widget
      lastBlynkAlertTime = currentTime;
      blynkAlertActive = true;
    }
  } else {
    if (blynkAlertActive) { // Nếu trước đó có cảnh báo, giờ thì không
      Blynk.virtualWrite(VPIN_TERMINAL_MSG, "Hệ thống ổn định.");
      // Cân nhắc gửi logEvent "system_normal" nếu cần
    }
    blynkAlertActive = false;
  }
}

//=============== CALLBACK KHI NHẬN DỮ LIỆU ESP-NOW ===============
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(SensorData)) {
    SensorData receivedLocalData; // Tạo bản sao cục bộ để xử lý
    memcpy(&receivedLocalData, incomingData, sizeof(receivedLocalData));

    // Serial.printf("Received: DS18B20: %.1fC, Shake: %.2f\n", receivedLocalData.temp_drill, receivedLocalData.vibration);

    // Cập nhật giá trị lên Blynk Widgets
    if (Blynk.connected()) {
      Blynk.virtualWrite(VPIN_TEMP_DRILL, receivedLocalData.temp_drill);
      Blynk.virtualWrite(VPIN_TEMP_CENTER, receivedLocalData.temp_center);
      Blynk.virtualWrite(VPIN_TEMP_MINE, receivedLocalData.temp_mine);
      Blynk.virtualWrite(VPIN_VIBRATION, receivedLocalData.vibration);
      Blynk.virtualWrite(VPIN_GAS_MQ2, receivedLocalData.gas_mq2);
      Blynk.virtualWrite(VPIN_GAS_MQ5, receivedLocalData.gas_mq5);
      Blynk.virtualWrite(VPIN_GAS_MQ9, receivedLocalData.gas_mq9);
      Blynk.virtualWrite(VPIN_GAS_MQ135, receivedLocalData.gas_mq135);
    }
    
    // Xử lý cảnh báo cục bộ và gửi thông báo Blynk
    handleReceiverAlertsAndBlynk(receivedLocalData);
  } else {
    Serial.printf("Error: Received data of wrong size. Expected %d, got %d bytes.\n", sizeof(SensorData), len);
  }
}

//=============== KHỞI TẠO HỆ THỐNG ===============
void setup() {
  Serial.begin(115200);
  Serial.println("Receiver initializing...");

  // --- Khởi tạo chân cảnh báo cục bộ ---
  pinMode(RECEIVER_ALARM_LED_PIN, OUTPUT);
  pinMode(RECEIVER_ALARM_BUZZER_PIN, OUTPUT);
  digitalWrite(RECEIVER_ALARM_LED_PIN, LOW);
  digitalWrite(RECEIVER_ALARM_BUZZER_PIN, LOW);

  // --- Khởi tạo WiFi và ESP-NOW ---
  WiFi.mode(WIFI_STA);
  // WiFi.disconnect(); // Không cần thiết nếu ngay sau là begin()

  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("Error setting WiFi channel");
    return;
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // --- Kết nối WiFi để dùng Blynk ---
  Serial.print("Connecting to WiFi: "); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  int connect_tries = 0;
  while (WiFi.status() != WL_CONNECTED && connect_tries < 20) { // Chờ tối đa 10 giây
    delay(500);
    Serial.print(".");
    connect_tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected.");
    Blynk.config(BLYNK_AUTH_TOKEN);
    if (Blynk.connect(5000)) { 
        Serial.println("Blynk Connected.");
    } else {
        Serial.println("ERROR: Blynk connection failed or timed out!");
    }
  } else {
    Serial.println("\nERROR: WiFi connection failed. Blynk will not be available.");
  }
  Serial.println("Receiver initialized. Waiting for data...");
}

//=============== VÒNG LẶP CHÍNH ===============
void loop() {
  if (WiFi.status() == WL_CONNECTED) { 
    if (Blynk.connected()) {
      Blynk.run();
    } else {
    }
  }
}
