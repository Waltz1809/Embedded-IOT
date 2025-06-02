#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"
#define BLYNK_AUTH_TOKEN "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"

#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h> // Vẫn cần cho sqrt nếu có tính toán khác, hoặc để đó cũng không sao

char ssid[] = "Phuong Hoa";
char pass[] = "65dienbienphu";

const int WIFI_CHANNEL = 11;

char latestMsgBuffer[251]; // Buffer đủ lớn cho các message tiềm năng
volatile bool newDataAvailable = false;
portMUX_TYPE onReceiveMux = portMUX_INITIALIZER_UNLOCKED;

// Ngưỡng và trạng thái cảnh báo nhiệt độ
const float TEMP_THRESHOLD_HIGH = 55.0;
const float TEMP_HYSTERESIS = 2.0; // Độ trễ để tránh cảnh báo liên tục
bool highTempAlertActive = false;
unsigned long lastHighTempAlertTime = 0;

// Ngưỡng và trạng thái cảnh báo rung lắc
const float SHAKE_MAGNITUDE_THRESHOLD = 15.0; // Ngưỡng rung
const float SHAKE_HYSTERESIS = 2.0;           // Độ trễ
bool shakeAlertActive = false;
unsigned long lastShakeAlertTime = 0;

const unsigned long NOTIFICATION_COOLDOWN = 5000; // 5 giây giữa các thông báo cùng loại

// Blynk Event codes (giữ nguyên)
const char* EVENT_CODE_HIGH_TEMP = "high_temp_alert";
const char* EVENT_CODE_SHAKE_ALERT = "shake_alert";
const char* EVENT_CODE_TEMP_NORMAL = "temp_normal";
const char* EVENT_CODE_SHAKE_STOPPED = "shake_stopped";

// Virtual Pins
const int VPIN_RAW_DATA = V0; // Dữ liệu thô nhận được (T:xx S:yy)
const int VPIN_TEMPERATURE = V1;
const int VPIN_SHAKE_MAGNITUDE = V2;


// Callback function khi nhận được dữ liệu ESP-NOW
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  portENTER_CRITICAL(&onReceiveMux);
  // Đảm bảo không ghi tràn buffer và có null terminator
  int copyLen = (len >= sizeof(latestMsgBuffer)) ? (sizeof(latestMsgBuffer) - 1) : len;
  memcpy(latestMsgBuffer, incomingData, copyLen);
  latestMsgBuffer[copyLen] = '\0'; // Luôn đảm bảo chuỗi kết thúc bằng NULL
  newDataAvailable = true;
  portEXIT_CRITICAL(&onReceiveMux);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Receiver] Starting...");
  memset(latestMsgBuffer, 0, sizeof(latestMsgBuffer));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Tắt WiFi trước khi cấu hình kênh
  delay(100);

  // Thiết lập kênh WiFi cho ESP-NOW (quan trọng)
  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("[Receiver] ERROR: Setting WiFi channel!");
    while (true) delay(1000); // Dừng nếu không thể đặt kênh
  }

  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[Receiver] ERROR: ESP-NOW init failed!");
    while (true) delay(1000); // Dừng nếu lỗi
  }

  // Đăng ký callback function cho việc nhận dữ liệu
  if (esp_now_register_recv_cb(onDataRecv) != ESP_OK) {
    Serial.println("[Receiver] ERROR: Failed to register ESP-NOW recv_cb!");
    while (true) delay(1000); // Dừng nếu lỗi
  }
  Serial.println("[Receiver] ESP-NOW Ready.");

  // Kết nối WiFi để dùng Blynk
  Serial.print("[Receiver] Connecting to WiFi: "); Serial.println(ssid);
  WiFi.begin(ssid, pass);
  int connect_tries = 0;
  const int max_connect_tries = 20; // Khoảng 10 giây
  while (WiFi.status() != WL_CONNECTED && connect_tries < max_connect_tries) {
    delay(500); Serial.print("."); connect_tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Receiver] WiFi Connected.");
    Blynk.config(BLYNK_AUTH_TOKEN);
    if (Blynk.connect(10000)) { // Timeout 10 giây cho kết nối Blynk
        Serial.println("[Receiver] Blynk Connected.");
    } else {
        Serial.println("[Receiver] ERROR: Blynk connection failed or timed out!");
    }
  } else {
    Serial.println("\n[Receiver] ERROR: WiFi connection failed. Blynk will not be available.");
  }
   Serial.println("[Receiver] Setup complete. Waiting for data...\n");
}

void processReceivedData() {
  char localMsgCopy[sizeof(latestMsgBuffer)];
  bool hasDataToProcess = false;

  // Sao chép dữ liệu nhận được một cách an toàn
  portENTER_CRITICAL(&onReceiveMux);
  if (newDataAvailable) {
    memcpy(localMsgCopy, latestMsgBuffer, sizeof(latestMsgBuffer)); // Sao chép toàn bộ buffer
    newDataAvailable = false; // Đặt lại cờ
    hasDataToProcess = true;
  }
  portEXIT_CRITICAL(&onReceiveMux);

  if (hasDataToProcess) {
    unsigned long currentTime = millis();

    // In dữ liệu thô nhận được từ ESP-NOW
    Serial.print("ESP-NOW Recv: \""); Serial.print(localMsgCopy); Serial.println("\"");

    float temp_recv = -999.0, shake_magnitude_recv = 0.0;
    // Định dạng mong đợi từ sender: "T:nhiet_do S:rung_lac"
    // Ví dụ: "T:25.5 S:18.7"
    int parsedItems = sscanf(localMsgCopy, "T:%f S:%f", &temp_recv, &shake_magnitude_recv);

    if (parsedItems == 2) { // Phải parse được 2 giá trị
      // Đã parse thành công, giờ có thể sử dụng temp_recv và shake_magnitude_recv
      // Serial.print("Parsed -> Temp: "); Serial.print(temp_recv, 1); // Xóa log này
      // Serial.print(" | Shake: "); Serial.println(shake_magnitude_recv, 1); // Xóa log này

      if (Blynk.connected()) {
        Blynk.virtualWrite(VPIN_RAW_DATA, localMsgCopy); // Gửi chuỗi gốc lên V0
        Blynk.virtualWrite(VPIN_TEMPERATURE, temp_recv); // Gửi nhiệt độ lên V1
        Blynk.virtualWrite(VPIN_SHAKE_MAGNITUDE, shake_magnitude_recv); // Gửi độ lớn rung lắc lên V2

        // 1. Kiểm tra nhiệt độ cao
        if (temp_recv > TEMP_THRESHOLD_HIGH && temp_recv != -999.0) {
          if (!highTempAlertActive || (currentTime - lastHighTempAlertTime > NOTIFICATION_COOLDOWN)) {
            String msg = "Nhiet do ham mo: " + String(temp_recv, 2) + "°C. NGUY HIEM!";
            Blynk.logEvent(EVENT_CODE_HIGH_TEMP, msg);
            highTempAlertActive = true;
            lastHighTempAlertTime = currentTime;
          }
        } else if (highTempAlertActive && temp_recv < (TEMP_THRESHOLD_HIGH - TEMP_HYSTERESIS)) {
           String msg = "Nhiet do ham mo da on dinh: " + String(temp_recv, 2) + "°C.";
           Blynk.logEvent(EVENT_CODE_TEMP_NORMAL, msg);
           highTempAlertActive = false;
        }

        // 2. Kiểm tra rung lắc mạnh
        bool isOverThreshold = shake_magnitude_recv > SHAKE_MAGNITUDE_THRESHOLD;
        bool canSendNotification = !shakeAlertActive || (currentTime - lastShakeAlertTime > NOTIFICATION_COOLDOWN);

        // Xóa các log kiểm tra chi tiết ở đây
        // Serial.print("  Shake Check: Value="); Serial.print(shake_magnitude_recv, 1);
        // Serial.print(" > Threshold("); Serial.print(SHAKE_MAGNITUDE_THRESHOLD, 1);
        // Serial.print(")? -> "); Serial.print(isOverThreshold ? "YES" : "NO");
        // Serial.print(" | Can Notify? -> "); Serial.println(canSendNotification ? "YES" : "NO");

        if (isOverThreshold) {
          if (canSendNotification) {
            String msg = "Phát hiện rung lắc mạnh! Chi so: " + String(shake_magnitude_recv, 2);
            // Serial.println("    >>> SENDING BLYNK SHAKE ALERT <<<"); // Xóa log này
            Blynk.logEvent(EVENT_CODE_SHAKE_ALERT, msg);
            shakeAlertActive = true;
            lastShakeAlertTime = currentTime;
          } // else { // Xóa log này
            // Serial.println("    Shake detected, but notification NOT sent (active or cooldown).");
          // }
        } else if (shakeAlertActive && shake_magnitude_recv < (SHAKE_MAGNITUDE_THRESHOLD - SHAKE_HYSTERESIS)) {
           String msg = "Rung lac da dung. Chi so: " + String(shake_magnitude_recv, 2);
           // Serial.println("    >>> SENDING BLYNK SHAKE STOPPED <<<"); // Xóa log này
           Blynk.logEvent(EVENT_CODE_SHAKE_STOPPED, msg);
           shakeAlertActive = false;
        }
      } else { // Blynk không kết nối
         // Serial.println("[Receiver] ERROR: Blynk not connected. Cannot process alerts."); // Có thể giữ lại nếu muốn biết lỗi này
      }
    } else { // sscanf thất bại
      // Serial.print("[Receiver] ERROR: Failed to parse ESP-NOW data. Raw: \""); // Có thể giữ lại nếu muốn debug
      // Serial.print(localMsgCopy); Serial.print("\", Parsed items: "); Serial.println(parsedItems);
    }
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!Blynk.connected()) {
      // Serial.println("[Loop] Blynk disconnected, attempting reconnect...");
      if (Blynk.connect(5000)) { // Thử kết nối lại với timeout ngắn hơn
          // Serial.println("[Loop] Blynk reconnected.");
      } // else {
          // Serial.println("[Loop] Blynk still not connected.");
      // }
    }
    if (Blynk.connected()) { // Chỉ chạy Blynk.run() nếu đã kết nối
        Blynk.run();
    }
  } // else {
    // Cân nhắc việc cố gắng kết nối lại WiFi nếu mất kết nối lâu
    // Serial.println("[Loop] WiFi disconnected. Trying to reconnect...");
    // WiFi.begin(ssid, pass);
    // delay(5000); // Chờ một chút
  // }

  processReceivedData(); // Luôn xử lý dữ liệu ESP-NOW nhận được
  // delay(10); // Thêm một delay nhỏ nếu cần thiết để tránh watchdog timer nếu loop quá rảnh
}

