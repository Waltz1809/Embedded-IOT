// --- BLYNK CONFIGURATION ---
#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"       // ID của Template Blynk
#define BLYNK_TEMPLATE_NAME "Testing"           // Tên Template Blynk
#define BLYNK_AUTH_TOKEN "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7" // Mã xác thực Blynk cho thiết bị này

// --- DEBUGGING AND LIBRARIES ---
#define BLYNK_PRINT Serial                    // Chuyển hướng các thông báo debug của Blynk ra Serial Monitor
#include <BlynkSimpleEsp32.h>                 // Thư viện Blynk cho ESP32
#include <WiFi.h>                             // Thư viện Wi-Fi cho ESP32
#include <esp_now.h>                          // Thư viện ESP-NOW để giao tiếp không dây trực tiếp
#include <esp_wifi.h>                         // Thư viện Wi-Fi cấp thấp hơn của ESP-IDF (dùng cho esp_now_init và channel)
#include <math.h>                             // Thư viện toán học (ví dụ: sqrt, không dùng ở đây nhưng có thể cần)

// --- WI-FI CREDENTIALS AND ESP-NOW CHANNEL ---
char ssid[] = "Phuong Hoa";                   // Tên mạng Wi-Fi (SSID) để kết nối Blynk
char pass[] = "65dienbienphu";                // Mật khẩu mạng Wi-Fi
const int WIFI_CHANNEL = 11;                  // Kênh Wi-Fi cho ESP-NOW (phải giống với Sender)

// --- RECEIVER HARDWARE PINS ---
#define RECEIVER_ALARM_LED_PIN 26             // Chân GPIO cho đèn LED cảnh báo trên Receiver
#define RECEIVER_ALARM_BUZZER_PIN 25          // Chân GPIO cho còi báo động trên Receiver

// --- DATA STRUCTURE FOR ESP-NOW ---
// Định nghĩa cấu trúc dữ liệu nhận được từ Sender. Phải khớp với cấu trúc ở Sender.
struct SensorData {
  float temp_drill;   // Nhiệt độ mũi khoan (từ DS18B20)
  float temp_center;  // Nhiệt độ khu vực trung tâm (từ DHT11)
  float temp_mine;    // Nhiệt độ khu vực mỏ (từ DHT22)
  float vibration;    // Độ lớn rung động (từ MPU6050)
  int gas_mq2;        // Giá trị đọc từ MQ-2 (khí cháy, khói)
  int gas_mq5;        // Giá trị đọc từ MQ-5 (LPG, khí tự nhiên)
  int gas_mq9;        // Giá trị đọc từ MQ-9 (CO, khí cháy)
  int gas_mq135;      // Giá trị đọc từ MQ-135 (chất lượng không khí, NH3, NOx, CO2...)
};

// --- RECEIVER ALERT THRESHOLDS ---
// Định nghĩa các ngưỡng cảnh báo trên Receiver. Nếu giá trị cảm biến vượt ngưỡng này, cảnh báo sẽ được kích hoạt.
const struct ReceiverAlertThresholds {
  float temp_drill_critical = 70.0;     // Ngưỡng nhiệt độ mũi khoan nguy hiểm
  float temp_center_critical = 40.0;    // Ngưỡng nhiệt độ trung tâm nguy hiểm
  float temp_mine_critical = 40.0;      // Ngưỡng nhiệt độ khu mỏ nguy hiểm
  float vibration_severe = 18.0;        // Ngưỡng rung động mạnh
  int gas_mq2_severe = 800;             // Ngưỡng khí MQ-2 nguy hiểm
  int gas_mq5_severe = 1000;            // Ngưỡng khí MQ-5 nguy hiểm
  int gas_mq9_severe = 1500;            // Ngưỡng khí MQ-9 nguy hiểm
  int gas_mq135_severe = 1500;          // Ngưỡng khí MQ-135 nguy hiểm
} RCV_THRESHOLDS; // Tạo một instance của cấu trúc ngưỡng

// --- BLYNK NOTIFICATION STATE VARIABLES ---
bool blynkAlertActive = false;                      // Cờ theo dõi trạng thái cảnh báo Blynk (đã gửi hay chưa)
unsigned long lastBlynkAlertTime = 0;               // Thời điểm cuối cùng gửi thông báo Blynk
const unsigned long BLYNK_NOTIFICATION_COOLDOWN = 3000; // Thời gian chờ tối thiểu (ms) giữa các thông báo Blynk để tránh spam (3 giây)

// --- BLYNK VIRTUAL PINS DEFINITIONS ---
// Định nghĩa các Virtual Pin trên ứng dụng Blynk để hiển thị dữ liệu
#define VPIN_TEMP_DRILL    V1   // Virtual Pin cho nhiệt độ mũi khoan
#define VPIN_TEMP_CENTER   V2   // Virtual Pin cho nhiệt độ trung tâm
#define VPIN_TEMP_MINE     V3   // Virtual Pin cho nhiệt độ khu mỏ
#define VPIN_VIBRATION     V4   // Virtual Pin cho rung động
#define VPIN_GAS_MQ2       V5   // Virtual Pin cho khí MQ-2
#define VPIN_GAS_MQ5       V6   // Virtual Pin cho khí MQ-5
#define VPIN_GAS_MQ9       V7   // Virtual Pin cho khí MQ-9
#define VPIN_GAS_MQ135     V8   // Virtual Pin cho khí MQ-135
#define VPIN_TERMINAL_MSG  V0   // Virtual Pin cho widget Terminal trên Blynk (hiển thị thông báo)

// --- FUNCTION: printReceivedData ---
// Chức năng: In dữ liệu cảm biến nhận được ra Serial Monitor để debug.
// Tham số:
//   - data: Tham chiếu đến cấu trúc SensorData chứa dữ liệu nhận được.
void printReceivedData(const SensorData &data) {
  Serial.print("RCV: "); // Tiền tố cho biết đây là log từ Receiver
  // In từng giá trị cảm biến với định dạng rõ ràng
  Serial.print("Drill:"); Serial.print(data.temp_drill, 1); // Nhiệt độ mũi khoan, 1 chữ số sau dấu phẩy
  Serial.print("C Cntr(DHT11):"); Serial.print(data.temp_center, 1); // Nhiệt độ trung tâm (DHT11), 1 chữ số sau dấu phẩy
  Serial.print("C Mine(DHT22):"); Serial.print(data.temp_mine, 1); // Nhiệt độ khu mỏ (DHT22), 1 chữ số sau dấu phẩy
  Serial.print("C Vib:"); Serial.print(data.vibration, 1); // Rung động, 1 chữ số sau dấu phẩy
  Serial.print(" MQ2:"); Serial.print(data.gas_mq2);       // Giá trị MQ-2
  Serial.print(" MQ5:"); Serial.print(data.gas_mq5);       // Giá trị MQ-5
  Serial.print(" MQ9:"); Serial.print(data.gas_mq9);       // Giá trị MQ-9
  Serial.print(" MQ135:"); Serial.print(data.gas_mq135);   // Giá trị MQ-135
  Serial.println(); // Xuống dòng mới
}

// --- FUNCTION: handleReceiverAlertsAndBlynk ---
// Chức năng: Xử lý dữ liệu cảm biến nhận được, kiểm tra các ngưỡng cảnh báo,
//            kích hoạt cảnh báo cục bộ (LED, còi) và gửi thông báo/dữ liệu lên Blynk.
// Tham số:
//   - data: Tham chiếu đến cấu trúc SensorData chứa dữ liệu nhận được.
void handleReceiverAlertsAndBlynk(const SensorData &data) {
  bool localAlertState = false; // Cờ trạng thái cảnh báo cục bộ, ban đầu là không có cảnh báo
  String alertMessage = "";     // Chuỗi chứa thông điệp cảnh báo để gửi lên Blynk

  // Kiểm tra ngưỡng nhiệt độ mũi khoan
  if (data.temp_drill >= RCV_THRESHOLDS.temp_drill_critical && data.temp_drill != -999.0) { // Nếu nhiệt độ vượt ngưỡng VÀ không phải giá trị lỗi
    alertMessage += "Mũi khoan nóng (" + String(data.temp_drill, 1) + "C)! "; // Thêm vào thông điệp cảnh báo
    localAlertState = true; // Đặt cờ cảnh báo cục bộ
  }

  // Kiểm tra ngưỡng nhiệt độ trung tâm (DHT11)
  if (data.temp_center >= RCV_THRESHOLDS.temp_center_critical && data.temp_center != -999.0) {
    alertMessage += "Trung tâm(DHT11) nóng (" + String(data.temp_center, 1) + "C)! ";
    localAlertState = true;
  }

  // Kiểm tra ngưỡng nhiệt độ khu mỏ (DHT22)
  if (data.temp_mine >= RCV_THRESHOLDS.temp_mine_critical && data.temp_mine != -999.0) {
    alertMessage += "Khu mỏ(DHT22) nóng (" + String(data.temp_mine, 1) + "C)! ";
    localAlertState = true;
  }

  // Kiểm tra ngưỡng rung động
  if (data.vibration >= RCV_THRESHOLDS.vibration_severe && data.vibration != -999.0) {
    alertMessage += "Rung mạnh (" + String(data.vibration, 1) + ")! ";
    localAlertState = true;
  }

  // Xử lý cảnh báo khí (gom các cảnh báo khí lại)
  String gasSpecificAlerts = ""; // Chuỗi chứa thông tin chi tiết về các khí vượt ngưỡng
  bool anyGasAlert = false;      // Cờ cho biết có bất kỳ cảnh báo khí nào không

  // Kiểm tra ngưỡng khí MQ-2
  if (data.gas_mq2 >= RCV_THRESHOLDS.gas_mq2_severe && data.gas_mq2 != -999) {
    gasSpecificAlerts += "MQ2:" + String(data.gas_mq2) + " ";
    anyGasAlert = true;
  }
  // Kiểm tra ngưỡng khí MQ-5
  if (data.gas_mq5 >= RCV_THRESHOLDS.gas_mq5_severe && data.gas_mq5 != -999) {
    gasSpecificAlerts += "MQ5:" + String(data.gas_mq5) + " ";
    anyGasAlert = true;
  }
  // Kiểm tra ngưỡng khí MQ-9
  if (data.gas_mq9 >= RCV_THRESHOLDS.gas_mq9_severe && data.gas_mq9 != -999) {
    gasSpecificAlerts += "MQ9:" + String(data.gas_mq9) + " ";
    anyGasAlert = true;
  }
  // Kiểm tra ngưỡng khí MQ-135
  if (data.gas_mq135 >= RCV_THRESHOLDS.gas_mq135_severe && data.gas_mq135 != -999) {
    gasSpecificAlerts += "MQ135:" + String(data.gas_mq135) + " ";
    anyGasAlert = true;
  }

  // Nếu có bất kỳ cảnh báo khí nào
  if (anyGasAlert) {
    gasSpecificAlerts.trim(); // Loại bỏ khoảng trắng thừa ở đầu/cuối chuỗi chi tiết khí
    alertMessage += "Nguy cơ khí độc! (" + gasSpecificAlerts + ") "; // Thêm thông điệp chung về khí độc và chi tiết
    localAlertState = true; // Đặt cờ cảnh báo cục bộ
  }

  // Kích hoạt hoặc tắt LED và còi báo động cục bộ trên Receiver
  digitalWrite(RECEIVER_ALARM_LED_PIN, localAlertState);    // Bật/tắt LED
  digitalWrite(RECEIVER_ALARM_BUZZER_PIN, localAlertState); // Bật/tắt còi

  // Xử lý gửi thông báo lên Blynk (với cơ chế cooldown)
  unsigned long currentTime = millis(); // Lấy thời gian hiện tại
  if (localAlertState) { // Nếu có cảnh báo cục bộ
    // Chỉ gửi thông báo Blynk nếu:
    // 1. Đây là cảnh báo mới (blynkAlertActive == false) HOẶC
    // 2. Đã qua thời gian cooldown kể từ lần gửi thông báo trước
    if (!blynkAlertActive || (currentTime - lastBlynkAlertTime > BLYNK_NOTIFICATION_COOLDOWN)) {
      Serial.println("BLYNK ALERT: " + alertMessage); // In thông báo ra Serial
      // Gửi sự kiện "critical_alert" lên Blynk (dùng cho Push Notification)
      // Giới hạn độ dài thông báo gửi đi để tránh lỗi (Blynk có giới hạn)
      Blynk.logEvent("critical_alert", alertMessage.substring(0, min((int)alertMessage.length(), 250)));
      // Ghi thông báo lên Terminal Widget trên Blynk
      Blynk.virtualWrite(VPIN_TERMINAL_MSG, alertMessage);
      lastBlynkAlertTime = currentTime; // Cập nhật thời điểm gửi thông báo cuối cùng
      blynkAlertActive = true;          // Đặt cờ cảnh báo Blynk đang hoạt động
    }
  } else { // Nếu không có cảnh báo cục bộ
    if (blynkAlertActive) { // Nếu trước đó có cảnh báo Blynk đang hoạt động
      // Gửi thông báo "Hệ thống ổn định" lên Terminal Blynk
      Blynk.virtualWrite(VPIN_TERMINAL_MSG, "Hệ thống ổn định.");
    }
    blynkAlertActive = false; // Đặt lại cờ cảnh báo Blynk
  }
}

// --- ESP-NOW CALLBACK FUNCTION: OnDataRecv ---
// Chức năng: Hàm callback được gọi tự động khi có dữ liệu ESP-NOW được nhận từ Sender.
// Tham số:
//   - info: Con trỏ chứa thông tin về người gửi (MAC address, etc.) - ít dùng trong trường hợp này.
//   - incomingData: Con trỏ đến mảng byte chứa dữ liệu nhận được.
//   - len: Độ dài của dữ liệu nhận được (tính bằng byte).
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  // Kiểm tra xem độ dài dữ liệu nhận được có khớp với kích thước của cấu trúc SensorData không
  if (len == sizeof(SensorData)) {
    SensorData receivedLocalData; // Tạo một biến cục bộ để lưu dữ liệu đã giải mã
    // Sao chép dữ liệu từ mảng byte nhận được vào cấu trúc SensorData
    memcpy(&receivedLocalData, incomingData, sizeof(receivedLocalData));
    printReceivedData(receivedLocalData); // In dữ liệu nhận được ra Serial Monitor

    // Gửi các giá trị cảm biến lên các Virtual Pin tương ứng trên Blynk
    Blynk.virtualWrite(VPIN_TEMP_DRILL, receivedLocalData.temp_drill);
    Blynk.virtualWrite(VPIN_TEMP_CENTER, receivedLocalData.temp_center);
    Blynk.virtualWrite(VPIN_TEMP_MINE, receivedLocalData.temp_mine);
    Blynk.virtualWrite(VPIN_VIBRATION, receivedLocalData.vibration);
    Blynk.virtualWrite(VPIN_GAS_MQ2, receivedLocalData.gas_mq2);
    Blynk.virtualWrite(VPIN_GAS_MQ5, receivedLocalData.gas_mq5);
    Blynk.virtualWrite(VPIN_GAS_MQ9, receivedLocalData.gas_mq9);
    Blynk.virtualWrite(VPIN_GAS_MQ135, receivedLocalData.gas_mq135);

    // Xử lý các cảnh báo và gửi thông báo Blynk dựa trên dữ liệu vừa nhận
    handleReceiverAlertsAndBlynk(receivedLocalData);
  }
}

// --- SETUP FUNCTION ---
// Chức năng: Hàm cài đặt, chạy một lần khi ESP32 khởi động hoặc reset.
void setup() {
  Serial.begin(115200); // Khởi tạo giao tiếp Serial ở tốc độ 115200 baud

  // Cài đặt chân GPIO cho LED và còi báo động cục bộ là OUTPUT
  pinMode(RECEIVER_ALARM_LED_PIN, OUTPUT);
  pinMode(RECEIVER_ALARM_BUZZER_PIN, OUTPUT);
  // Đảm bảo LED và còi tắt khi khởi động
  digitalWrite(RECEIVER_ALARM_LED_PIN, LOW);
  digitalWrite(RECEIVER_ALARM_BUZZER_PIN, LOW);

  // --- ESP-NOW INITIALIZATION ---
  WiFi.mode(WIFI_STA); // Đặt ESP32 ở chế độ Station (cần thiết cho ESP-NOW và kết nối Wi-Fi sau này)
  // Đặt kênh Wi-Fi cho ESP-NOW. Rất quan trọng, phải giống với kênh của Sender.
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi khởi tạo ESP-NOW!"); // In thông báo lỗi nếu khởi tạo thất bại
    return; // Thoát khỏi hàm setup nếu lỗi
  }
  // Đăng ký hàm callback OnDataRecv để xử lý dữ liệu nhận được qua ESP-NOW
  esp_now_register_recv_cb(OnDataRecv);

  // --- BLYNK INITIALIZATION ---
  WiFi.begin(ssid, pass); // Bắt đầu kết nối vào mạng Wi-Fi đã khai báo
  delay(5000);            // Chờ 5 giây để Wi-Fi kết nối (có thể cải thiện bằng vòng lặp kiểm tra)
  Blynk.config(BLYNK_AUTH_TOKEN); // Cấu hình Blynk với mã xác thực
  Blynk.connect(5000);            // Kết nối đến server Blynk với timeout 5 giây
}

// --- LOOP FUNCTION ---
// Chức năng: Hàm lặp chính, chạy liên tục sau khi hàm setup() hoàn thành.
void loop() {
  // Chỉ chạy các hàm của Blynk nếu đã kết nối Wi-Fi thành công
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run(); // Xử lý các tác vụ của Blynk (nhận lệnh, gửi dữ liệu, duy trì kết nối)
  }
}
