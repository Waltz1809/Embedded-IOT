// --- LIBRARIES ---
#include <esp_now.h>            // Thư viện ESP-NOW để giao tiếp không dây trực tiếp
#include <WiFi.h>               // Thư viện Wi-Fi cho ESP32 (cần cho esp_now)
#include <esp_wifi.h>           // Thư viện Wi-Fi cấp thấp hơn của ESP-IDF (dùng cho esp_now_init và channel)
#include <Wire.h>               // Thư viện cho giao tiếp I2C (dùng cho MPU6050)
#include <Adafruit_MPU6050.h>   // Thư viện cho cảm biến MPU6050
#include <Adafruit_Sensor.h>    // Thư viện cảm biến cơ bản của Adafruit (MPU6050 cần)
#include <OneWire.h>            // Thư viện cho giao tiếp OneWire (dùng cho DS18B20)
#include <DallasTemperature.h>  // Thư viện cho cảm biến nhiệt độ DS18B20
#include <DHT.h>                // Thư viện cho cảm biến nhiệt độ và độ ẩm DHT
#include <math.h>               // Thư viện toán học (dùng cho sqrt để tính rung động)

// --- HARDWARE PIN DEFINITIONS (SENSORS) ---
#define DS18B20_PIN 25          // Chân GPIO cho cảm biến nhiệt độ DS18B20 (OneWire data)
#define DHT11_CENTER_PIN 17     // Chân GPIO cho cảm biến DHT11 (khu vực trung tâm)
#define DHT22_MINE_PIN   22     // Chân GPIO cho cảm biến DHT22 (khu vực mỏ)
#define I2C_SDA_PIN 27          // Chân GPIO cho SDA của giao tiếp I2C (MPU6050)
#define I2C_SCL_PIN 26          // Chân GPIO cho SCL của giao tiếp I2C (MPU6050)

// --- HARDWARE PIN DEFINITIONS (GAS SENSORS - ANALOG) ---
#define MQ2_PIN   32            // Chân Analog GPIO cho cảm biến khí MQ-2
#define MQ5_PIN   33            // Chân Analog GPIO cho cảm biến khí MQ-5
#define MQ9_PIN   14            // Chân Analog GPIO cho cảm biến khí MQ-9
#define MQ135_PIN 35            // Chân Analog GPIO cho cảm biến khí MQ-135

// --- HARDWARE PIN DEFINITIONS (LOCAL ALERTS ON SENDER) ---
#define SENDER_BUZZER_PIN 15    // Chân GPIO cho còi báo động cục bộ trên Sender
#define SENDER_STATUS_LED_PIN 12 // Chân GPIO cho đèn LED trạng thái/cảnh báo cục bộ trên Sender

// --- ESP-NOW CONFIGURATION ---
const int WIFI_CHANNEL = 11;    // Kênh Wi-Fi cho ESP-NOW (phải giống với Receiver)
// Địa chỉ MAC của Node Receiver. Quan trọng: Phải thay thế bằng địa chỉ MAC thực của ESP32 Receiver.
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC}; // Ví dụ MAC, cần thay đổi

// --- SENSOR OBJECTS INITIALIZATION ---
OneWire oneWire(DS18B20_PIN);                 // Khởi tạo đối tượng OneWire cho DS18B20
DallasTemperature ds18b20(&oneWire);        // Khởi tạo đối tượng DallasTemperature cho DS18B20
DHT dht11_center(DHT11_CENTER_PIN, DHT11);    // Khởi tạo đối tượng DHT11
DHT dht22_mine(DHT22_MINE_PIN, DHT22);      // Khởi tạo đối tượng DHT22
Adafruit_MPU6050 mpu;                       // Khởi tạo đối tượng MPU6050

// --- DATA STRUCTURE FOR ESP-NOW ---
// Định nghĩa cấu trúc dữ liệu để gửi đi. Phải khớp với cấu trúc ở Receiver.
struct SensorData {
  float temp_drill;   // Nhiệt độ mũi khoan (từ DS18B20)
  float temp_center;  // Nhiệt độ khu vực trung tâm (từ DHT11)
  float temp_mine;    // Nhiệt độ khu vực mỏ (từ DHT22)
  float vibration;    // Độ lớn rung động (từ MPU6050)
  int gas_mq2;        // Giá trị đọc từ MQ-2
  int gas_mq5;        // Giá trị đọc từ MQ-5
  int gas_mq9;        // Giá trị đọc từ MQ-9
  int gas_mq135;      // Giá trị đọc từ MQ-135
};
SensorData currentSensorData; // Biến toàn cục để lưu trữ dữ liệu cảm biến hiện tại sẽ được gửi đi

// --- TIMING FOR SENDING DATA ---
unsigned long lastSendTime = 0;       // Thời điểm cuối cùng gửi dữ liệu
const long sendInterval = 1000;       // Khoảng thời gian giữa các lần gửi dữ liệu (ms), ví dụ 1 giây

// --- SENDER LOCAL ALERT THRESHOLDS ---
// Định nghĩa các ngưỡng cảnh báo cục bộ trên Sender.
// Nếu giá trị cảm biến vượt ngưỡng này, còi và LED trên Sender sẽ được kích hoạt.
// --- THAY ĐỔI NGƯỠNG GAS Ở ĐÂY ---
const struct SenderAlertThresholds {
  float temp_drill_critical = 70.0;     // Ngưỡng nhiệt độ mũi khoan nguy hiểm
  float temp_center_critical = 40.0;    // Ngưỡng nhiệt độ trung tâm nguy hiểm
  float temp_mine_critical = 40.0;      // Ngưỡng nhiệt độ khu mỏ nguy hiểm
  float vibration_severe = 18.0;        // Ngưỡng rung động mạnh
  int gas_mq2_severe = 800;             // Ngưỡng cho MQ-2 (ví dụ: nền 300-400, báo động 600-700)
  int gas_mq5_severe = 1000;            // Ngưỡng cho MQ-5 (ví dụ: nền 600-700, báo động >1000)
  int gas_mq9_severe = 1500;            // Ngưỡng tạm thời cho MQ-9 (cần điều chỉnh dựa trên thực tế)
  int gas_mq135_severe = 1500;          // Ngưỡng cho MQ-135 (ví dụ: nền 1000, báo động >2000)
} SENDER_THRESHOLDS; // Tạo một instance của cấu trúc ngưỡng
// --- KẾT THÚC THAY ĐỔI NGƯỠNG GAS ---

// --- ESP-NOW CALLBACK FUNCTION: OnDataSent ---
// Chức năng: Hàm callback được gọi tự động sau khi dữ liệu ESP-NOW được gửi đi.
// Tham số:
//   - mac_addr: Địa chỉ MAC của thiết bị nhận.
//   - status: Trạng thái của việc gửi dữ liệu (thành công hay thất bại).
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gud" : "Nah"); // In trạng thái: "Gud" nếu thành công, "Nah" nếu thất bại
}

// --- FUNCTION: checkAndTriggerLocalSenderAlerts ---
// Chức năng: Kiểm tra dữ liệu cảm biến hiện tại với các ngưỡng cảnh báo cục bộ của Sender
//            và kích hoạt/tắt LED, còi trên Sender tương ứng.
// --- CẬP NHẬT LOGIC KIỂM TRA NGƯỠNG GAS ---
void checkAndTriggerLocalSenderAlerts() {
  bool alertState = false; // Cờ trạng thái cảnh báo cục bộ, ban đầu là không có cảnh báo

  // Kiểm tra tất cả các điều kiện cảnh báo
  // Nếu bất kỳ giá trị cảm biến nào vượt ngưỡng (và không phải là giá trị lỗi cho khí)
  if (currentSensorData.temp_drill >= SENDER_THRESHOLDS.temp_drill_critical ||
      currentSensorData.temp_center >= SENDER_THRESHOLDS.temp_center_critical ||
      currentSensorData.temp_mine >= SENDER_THRESHOLDS.temp_mine_critical ||
      currentSensorData.vibration >= SENDER_THRESHOLDS.vibration_severe ||
      (currentSensorData.gas_mq2 >= SENDER_THRESHOLDS.gas_mq2_severe && currentSensorData.gas_mq2 != -999) ||     // Kiểm tra MQ2 và không phải lỗi
      (currentSensorData.gas_mq5 >= SENDER_THRESHOLDS.gas_mq5_severe && currentSensorData.gas_mq5 != -999) ||     // Kiểm tra MQ5 và không phải lỗi
      (currentSensorData.gas_mq9 >= SENDER_THRESHOLDS.gas_mq9_severe && currentSensorData.gas_mq9 != -999) ||     // Kiểm tra MQ9 và không phải lỗi
      (currentSensorData.gas_mq135 >= SENDER_THRESHOLDS.gas_mq135_severe && currentSensorData.gas_mq135 != -999)) // Kiểm tra MQ135 và không phải lỗi
  {
    alertState = true; // Đặt cờ cảnh báo cục bộ
  }
  // Kích hoạt hoặc tắt LED và còi báo động cục bộ trên Sender
  digitalWrite(SENDER_BUZZER_PIN, alertState);    // Bật/tắt còi
  digitalWrite(SENDER_STATUS_LED_PIN, alertState); // Bật/tắt LED
}
// --- KẾT THÚC CẬP NHẬT LOGIC KIỂM TRA NGƯỠNG GAS ---

// --- FUNCTION: printSensorDataToSend ---
// Chức năng: In dữ liệu cảm biến (chuẩn bị gửi) ra Serial Monitor để debug.
// Tham số:
//   - data: Tham chiếu đến cấu trúc SensorData chứa dữ liệu sẽ gửi.
void printSensorDataToSend(const SensorData &data) {
  Serial.print("SND: "); // Tiền tố cho biết đây là log từ Sender
  // In từng giá trị cảm biến với định dạng rõ ràng
  Serial.print("Drill:"); Serial.print(data.temp_drill, 1);
  Serial.print("C Cntr(DHT11):"); Serial.print(data.temp_center, 1);
  Serial.print("C Mine(DHT22):"); Serial.print(data.temp_mine, 1);
  Serial.print("C Vib:"); Serial.print(data.vibration, 1);
  Serial.print(" MQ2:"); Serial.print(data.gas_mq2);
  Serial.print(" MQ5:"); Serial.print(data.gas_mq5);
  Serial.print(" MQ9:"); Serial.print(data.gas_mq9);
  Serial.print(" MQ135:"); Serial.print(data.gas_mq135);
  Serial.println(); // Xuống dòng mới
}

// --- SETUP FUNCTION ---
// Chức năng: Hàm cài đặt, chạy một lần khi ESP32 khởi động hoặc reset.
void setup() {
  Serial.begin(115200); // Khởi tạo giao tiếp Serial ở tốc độ 115200 baud

  // Khởi tạo giao tiếp I2C với các chân SDA, SCL đã định nghĩa (cho MPU6050)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // --- SENSOR INITIALIZATION ---
  ds18b20.begin();      // Khởi tạo cảm biến DS18B20
  dht11_center.begin(); // Khởi tạo cảm biến DHT11
  dht22_mine.begin();   // Khởi tạo cảm biến DHT22

  // Khởi tạo cảm biến MPU6050
  if (!mpu.begin(0x68, &Wire)) { // Thử khởi tạo MPU6050 ở địa chỉ I2C 0x68
    Serial.println("Không tìm thấy MPU6050!"); // In thông báo lỗi nếu không tìm thấy
    // currentSensorData.vibration = -999.0; // Có thể gán giá trị lỗi ở đây nếu muốn xử lý ngay
  } else {
    Serial.println("MPU6050 tìm thấy!");
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);  // Đặt dải đo gia tốc +/- 2G
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);     // Đặt dải đo con quay +/- 500 độ/giây
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);  // Đặt băng thông bộ lọc (giảm nhiễu)
  }
  
  // Cài đặt chân GPIO cho các cảm biến khí là INPUT (chúng là cảm biến analog)
  pinMode(MQ2_PIN, INPUT);
  pinMode(MQ5_PIN, INPUT);
  pinMode(MQ9_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  // Cài đặt chân GPIO cho LED và còi báo động cục bộ trên Sender là OUTPUT
  pinMode(SENDER_BUZZER_PIN, OUTPUT);
  pinMode(SENDER_STATUS_LED_PIN, OUTPUT);
  // Đảm bảo LED và còi tắt khi khởi động
  digitalWrite(SENDER_BUZZER_PIN, LOW);
  digitalWrite(SENDER_STATUS_LED_PIN, LOW);

  // --- ESP-NOW INITIALIZATION ---
  WiFi.mode(WIFI_STA);      // Đặt ESP32 ở chế độ Station (cần thiết cho ESP-NOW)
  WiFi.disconnect();        // Ngắt kết nối khỏi bất kỳ mạng Wi-Fi nào nếu có (để đảm bảo ESP-NOW hoạt động tốt)

  // Đặt kênh Wi-Fi cho ESP-NOW. Rất quan trọng, phải giống với kênh của Receiver.
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi khởi tạo ESP-NOW"); // In thông báo lỗi nếu khởi tạo thất bại
    return; // Thoát khỏi hàm setup nếu lỗi
  }
  // Đăng ký hàm callback OnDataSent để xử lý trạng thái sau khi gửi dữ liệu
  esp_now_register_send_cb(OnDataSent);

  // --- ADD RECEIVER AS ESP-NOW PEER ---
  esp_now_peer_info_t peerInfo = {}; // Tạo cấu trúc thông tin peer
  memcpy(peerInfo.peer_addr, receiverMAC, 6); // Sao chép địa chỉ MAC của Receiver vào cấu trúc peer
  peerInfo.channel = WIFI_CHANNEL;  // Đặt kênh giao tiếp (nên là 0 để tự động theo kênh hiện tại của ESP32, hoặc kênh đã set)
  peerInfo.encrypt = false;         // Không sử dụng mã hóa cho ESP-NOW (để đơn giản)
  peerInfo.ifidx = WIFI_IF_STA;     // Giao diện Wi-Fi sử dụng là Station
  
  // Thêm peer (Receiver) vào danh sách ESP-NOW
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Lỗi thêm peer"); // In thông báo lỗi nếu thêm peer thất bại
    return; // Thoát khỏi hàm setup nếu lỗi
  }
}

// --- LOOP FUNCTION ---
// Chức năng: Hàm lặp chính, chạy liên tục sau khi hàm setup() hoàn thành.
void loop() {
  // Kiểm tra xem đã đến lúc gửi dữ liệu chưa (dựa trên sendInterval)
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis(); // Cập nhật thời điểm gửi dữ liệu cuối cùng

    // --- READ SENSOR DATA ---
    // Đọc nhiệt độ từ DS18B20
    ds18b20.requestTemperatures(); // Yêu cầu cảm biến đọc nhiệt độ
    currentSensorData.temp_drill = ds18b20.getTempCByIndex(0); // Lấy nhiệt độ (độ C) từ cảm biến đầu tiên trên bus
    // Kiểm tra lỗi đọc DS18B20 (DEVICE_DISCONNECTED_C là -127, 85.0 là giá trị lỗi phổ biến khi mới khởi động)
    if (currentSensorData.temp_drill == DEVICE_DISCONNECTED_C || currentSensorData.temp_drill == 85.0) {
        currentSensorData.temp_drill = -999.0; // Gán giá trị lỗi
    }

    // Đọc nhiệt độ từ DHT11 (khu vực trung tâm)
    currentSensorData.temp_center = dht11_center.readTemperature();
    if (isnan(currentSensorData.temp_center)) { // Kiểm tra nếu giá trị đọc là NaN (Not a Number - lỗi)
        currentSensorData.temp_center = -999.0; // Gán giá trị lỗi
    }

    // Đọc nhiệt độ từ DHT22 (khu vực mỏ)
    currentSensorData.temp_mine = dht22_mine.readTemperature();
    if (isnan(currentSensorData.temp_mine)) { // Kiểm tra nếu giá trị đọc là NaN
        currentSensorData.temp_mine = -999.0; // Gán giá trị lỗi
    }

    // Đọc dữ liệu từ MPU6050 (gia tốc và con quay)
    sensors_event_t a, g, temp_mpu; // Tạo các biến để lưu trữ sự kiện cảm biến
    if (mpu.getEvent(&a, &g, &temp_mpu)) { // Nếu đọc dữ liệu thành công
      // Tính toán độ lớn vector gia tốc tổng hợp (chỉ số rung động)
      currentSensorData.vibration = sqrt(a.acceleration.x * a.acceleration.x +
                                        a.acceleration.y * a.acceleration.y +
                                        a.acceleration.z * a.acceleration.z);
    } else {
      currentSensorData.vibration = -999.0; // Gán giá trị lỗi nếu không đọc được MPU6050
    }

    // Đọc giá trị analog từ các cảm biến khí MQ
    currentSensorData.gas_mq2 = analogRead(MQ2_PIN);
    currentSensorData.gas_mq5 = analogRead(MQ5_PIN);
    // currentSensorData.gas_mq9 = -999; // Gán cứng giá trị lỗi cho MQ9 nếu nó vẫn có vấn đề
    currentSensorData.gas_mq9 = analogRead(MQ9_PIN); // Hoặc đọc bình thường nếu đã sửa
    currentSensorData.gas_mq135 = analogRead(MQ135_PIN);
    
    // Xử lý các giá trị đọc ADC từ cảm biến khí có thể không ổn định
    // Nếu đọc được 0 (chập GND) hoặc 4095 (chập VCC hoặc không kết nối) thì coi là lỗi
    if (currentSensorData.gas_mq2 == 0 || currentSensorData.gas_mq2 == 4095) currentSensorData.gas_mq2 = -999;
    if (currentSensorData.gas_mq5 == 0 || currentSensorData.gas_mq5 == 4095) currentSensorData.gas_mq5 = -999;
    if (currentSensorData.gas_mq9 == 0 || currentSensorData.gas_mq9 == 4095) currentSensorData.gas_mq9 = -999; // Áp dụng cho cả MQ9
    if (currentSensorData.gas_mq135 == 0 || currentSensorData.gas_mq135 == 4095) currentSensorData.gas_mq135 = -999;

    // --- PROCESS AND SEND DATA ---
    checkAndTriggerLocalSenderAlerts(); // Kiểm tra và kích hoạt cảnh báo cục bộ trên Sender
    printSensorDataToSend(currentSensorData); // In dữ liệu sẽ gửi ra Serial Monitor

    // Gửi dữ liệu cảm biến đến Receiver qua ESP-NOW
    // (uint8_t *) &currentSensorData: ép kiểu con trỏ của cấu trúc SensorData thành con trỏ mảng byte
    // sizeof(currentSensorData): kích thước của dữ liệu cần gửi
    esp_now_send(receiverMAC, (uint8_t *) &currentSensorData, sizeof(currentSensorData));
  }
}
