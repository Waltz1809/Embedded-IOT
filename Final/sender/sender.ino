@ -1,208 +1,122 @@
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// --- Chân kết nối cảm biến MQ ---
#define MQ2_PIN   34
#define MQ5_PIN   32
#define MQ9_PIN   33
#define MQ135_PIN 35

// --- Chân cảm biến khác ---
#define ONE_WIRE_BUS 25
const int I2C_SDA_PIN = 27;
const int I2C_SCL_PIN = 26;

// --- Cấu hình Wi-Fi và ESP-NOW ---
const int WIFI_CHANNEL = 11;
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC}; // MAC CỦA RECEIVER

// --- Khởi tạo đối tượng cảm biến ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_MPU6050 mpu;

// --- Biến toàn cục lưu giá trị cảm biến ---
float temperature = -999.0; // Khởi tạo với giá trị lỗi
float shakeMagnitude = 0.0;
float accX_raw, accY_raw, accZ_raw;

int mq2_raw_value = 0;
int mq5_raw_value = 0;
int mq9_raw_value = 0;
int mq135_raw_value = 0;

// --- Biến quản lý thời gian cho các tác vụ ---
unsigned long previousMainTaskTime = 0;
const long mainTaskInterval = 500; // MPU, MQ, ESP-NOW, Serial print mỗi 0.5 giây

unsigned long previousDs18b20Time = 0;
const long ds18b20Interval = 5000; // Đọc DS18B20 mỗi 5 giây (5000 ms)
bool ds18b20ReadyToRead = false;
unsigned long ds18b20ConversionStartTime = 0;
const int ds18b20ConversionTime = 750; // Thời gian chuyển đổi tối đa cho 12-bit

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print(millis());
    Serial.print(" -> Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gud" : "Nah");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  
  WiFi.disconnect(true, true); 
  delay(100);
  Serial.println("WiFi disconnected.");

  Serial.println("Setting WiFi channel...");
  esp_err_t channel_set_status = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channel_set_status != ESP_OK) {
    Serial.print("ERROR: Setting WiFi channel failed! Error: ");
    Serial.println(esp_err_to_name(channel_set_status));
    while (true) delay(1000);
  }
  Serial.println("WiFi channel set.");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("ERROR: Failed to find MPU6050.");
    while (true) delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  Serial.println("MPU6050 initialized.");

  sensors.begin();
  if (sensors.getDeviceCount() == 0) {
    Serial.println("WARNING: No DS18B20 sensors found.");
  } else {
    sensors.setResolution(12);
    sensors.setWaitForConversion(false); 
    Serial.println("DS18B20 initialized, non-blocking mode.");
  }

  Serial.println("Initializing ESP-NOW...");
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW initialization failed!");
    while (true) delay(1000);
  }
  Serial.println("ESP-NOW initialized.");

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  Serial.println("Adding/Modifying ESP-NOW peer...");
  if (esp_now_is_peer_exist(receiverMAC)) {
      esp_err_t modStatus = esp_now_mod_peer(&peerInfo);
       if (modStatus != ESP_OK) {
        Serial.print("ERROR: Failed to modify ESP-NOW peer: ");
        Serial.println(esp_err_to_name(modStatus));
        while (true) delay(1000);
      }
  } else {
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    if (addStatus != ESP_OK) {
      Serial.print("ERROR: Failed to add ESP-NOW peer: ");
      Serial.println(esp_err_to_name(addStatus));
      while (true) delay(1000);
    }
  }
  Serial.println("ESP-NOW peer configured.");
  esp_now_register_send_cb(OnDataSent);
  
  Serial.println("Warming up MQ sensors (15 seconds)...");
  delay(15000); // Bạn có thể tăng thời gian này nếu muốn
  Serial.println("MQ sensors warm-up finished (simulated).");
  
  Serial.println("Sender setup complete. Starting tasks...");
  previousMainTaskTime = millis();
  previousDs18b20Time = millis();

  if (sensors.getDeviceCount() > 0) {
      sensors.requestTemperatures();
      ds18b20ConversionStartTime = millis();
      ds18b20ReadyToRead = false; 
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Tác vụ chính: Đọc MPU, MQ, In Serial, Gửi ESP-NOW (mỗi mainTaskInterval) ---
  if (currentMillis - previousMainTaskTime >= mainTaskInterval) {
    previousMainTaskTime = currentMillis;

    // Đọc MPU6050
    sensors_event_t a, g, temp_mpu_event;
    if (mpu.getEvent(&a, &g, &temp_mpu_event)) {
      accX_raw = a.acceleration.x;
      accY_raw = a.acceleration.y;
      accZ_raw = a.acceleration.z;
      shakeMagnitude = sqrt(accX_raw * accX_raw + accY_raw * accY_raw + accZ_raw * accZ_raw); // Sửa lại công thức tính độ rung (nếu accZ quan trọng)
                                                                                // Hoặc dùng: sqrt(accX_raw*accX_raw + accY_raw*accY_raw + accZ_raw*accZ_raw)
    } else {
      accX_raw = 0; accY_raw = 0; accZ_raw = 0;
      shakeMagnitude = 0.0;
    }

    // Đọc các cảm biến MQ
    mq2_raw_value = analogRead(MQ2_PIN);
    mq5_raw_value = analogRead(MQ5_PIN);
    mq9_raw_value = analogRead(MQ9_PIN);
    mq135_raw_value = analogRead(MQ135_PIN);

    // In dữ liệu ra Serial
    Serial.print(currentMillis);
    Serial.print(" -> T:"); Serial.print(temperature, 1);
    Serial.print(", AccX:"); Serial.print(accX_raw, 2);
    Serial.print(", AccY:"); Serial.print(accY_raw, 2);
    Serial.print(", AccZ:"); Serial.print(accZ_raw, 2);
    Serial.print(", S:"); Serial.print(shakeMagnitude, 2);
    Serial.print(", MQ2:"); Serial.print(mq2_raw_value);
    Serial.print(", MQ5:"); Serial.print(mq5_raw_value);
    Serial.print(", MQ9:"); Serial.print(mq9_raw_value);
    Serial.print(", MQ135:"); Serial.println(mq135_raw_value);

    // Chuẩn bị message gửi qua ESP-NOW (chỉ Nhiệt độ và Độ rung)
    char msg[30]; 
    snprintf(msg, sizeof(msg), "T:%.1f S:%.1f", temperature, shakeMagnitude); 

    esp_now_send(receiverMAC, (uint8_t *)msg, strlen(msg));
    // Callback OnDataSent sẽ xử lý việc log trạng thái gửi
  }

  // --- Tác vụ đọc DS18B20 (ít thường xuyên hơn) ---
  if (sensors.getDeviceCount() > 0) {
    if (!ds18b20ReadyToRead && (currentMillis - previousDs18b20Time >= ds18b20Interval)) {
        sensors.requestTemperatures();
        ds18b20ConversionStartTime = currentMillis;
        previousDs18b20Time = currentMillis;
        ds18b20ReadyToRead = false;
        // Serial.print(currentMillis); // Có thể bỏ comment để debug
        // Serial.println(" -> DS18B20: Requested new temperature conversion.");
    }

    if (!ds18b20ReadyToRead && (currentMillis - ds18b20ConversionStartTime >= ds18b20ConversionTime)) {
        float tempRead = sensors.getTempCByIndex(0);
        if (tempRead == DEVICE_DISCONNECTED_C || tempRead == 85.0 || tempRead == -127.0) {
            // Giữ giá trị cũ hoặc báo lỗi
        } else {
            temperature = tempRead;
            // Serial.print(currentMillis); // Có thể bỏ comment để debug
            // Serial.print(" -> DS18B20: Temperature updated: ");
            // Serial.println(temperature, 1);
        }
        ds18b20ReadyToRead = true; 
    }
  }
}