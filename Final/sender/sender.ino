// === File: sender.ino ===
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Cấu hình DS18B20
#define ONE_WIRE_BUS 25 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_MPU6050 mpu;
float accX, accY, accZ, gyroX, gyroY, gyroZ;
float temperature = 0.0;
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC}; // Đổi theo MAC receiver thực tế

bool setupMPU() {
  // Khởi tạo I2C với pull-up
  Wire.begin(27, 26, 400000); // SDA=27, SCL=26, 400kHz
  delay(500); // Tăng delay
  
  Serial.println("Đang khởi tạo MPU6050...");
  
  // Thử nhiều lần
  for (int attempt = 0; attempt < 5; attempt++) {
    if (mpu.begin(0x68, &Wire)) { // Chỉ định địa chỉ I2C rõ ràng
      Serial.println("MPU6050 khởi tạo thành công!");
      
      // Cấu hình MPU
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu.setGyroRange(MPU6050_RANGE_500_DEG);
      mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
      
      // Test đọc dữ liệu
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      Serial.printf("Test MPU - AccX: %.2f, AccY: %.2f, AccZ: %.2f\n", 
                    a.acceleration.x, a.acceleration.y, a.acceleration.z);
      return true;
    }
    Serial.printf("Lần thử %d: MPU6050 không phản hồi\n", attempt + 1);
    delay(1000);
  }
  return false;
}

void updateMPU() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  accX = a.acceleration.x;
  accY = a.acceleration.y;
  accZ = a.acceleration.z;
  gyroX = g.gyro.x;
  gyroY = g.gyro.y;
  gyroZ = g.gyro.z;
  
  // Kiểm tra nếu tất cả giá trị = 0 (MPU bị lỗi)
  if (accX == 0 && accY == 0 && accZ == 0 && gyroX == 0 && gyroY == 0 && gyroZ == 0) {
    Serial.println("MPU ERROR: All values = 0, resetting I2C...");
    Wire.end();
    delay(100);
    Wire.begin(27, 26, 400000);
    delay(100);
    if (!mpu.begin(0x68, &Wire)) {
      Serial.println("MPU reset failed!");
    } else {
      mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
      mpu.setGyroRange(MPU6050_RANGE_500_DEG);
      mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
      Serial.println("MPU reset OK");
    }
  }
}

void updateDS18B20() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C) {
    temperature = -999.0; // Giá trị báo lỗi
  }
}

String serializeData() {
  // Format: accX,accY,accZ,gyroX,gyroY,gyroZ,temperature
  return String(accX, 2) + "," + String(accY, 2) + "," + String(accZ, 2) + "," +
         String(gyroX, 2) + "," + String(gyroY, 2) + "," + String(gyroZ, 2) + "," +
         String(temperature, 2);
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Gửi đến ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.print(" - Trạng thái: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "THÀNH CÔNG" : "THẤT BẠI");
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Tăng delay để đảm bảo serial sẵn sàng
  
  // Đặt WiFi mode trước khi lấy MAC
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // Đảm bảo không kết nối WiFi
  delay(1000); // Đợi WiFi khởi tạo
  
  Serial.println("=== ESP32 SENDER STARTING ===");
  Serial.println("MAC: " + WiFi.macAddress());
  Serial.print("Target Receiver MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", receiverMAC[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Khởi tạo DS18B20
  sensors.begin();
  Serial.println("DS18B20: 1");

  if (!setupMPU()) {
    Serial.println("MPU6050: 0 - CRITICAL ERROR!");
    // Không dừng hoàn toàn, cho phép tiếp tục với DS18B20
  } else {
    Serial.println("MPU6050: 1");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW: 0");
    return;
  }
  Serial.println("ESP-NOW: 1");

  // Đăng ký callback gửi dữ liệu
  esp_now_register_send_cb(onDataSent);

  // Cấu hình peer với interface rõ ràng
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo)); // Clear toàn bộ struct
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 1;  // Đặt channel cụ thể (1-14)
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA; // Đặt rõ interface

  esp_err_t addStatus = esp_now_add_peer(&peerInfo);
  if (addStatus == ESP_OK) {
    Serial.println("Receiver: 1");
  } else {
    Serial.printf("Receiver: 0 - Error code: %d\n", addStatus);
    Serial.println("Codes: ESP_ERR_ESPNOW_NOT_INIT=0x306A, ESP_ERR_ESPNOW_ARG=0x306B, ESP_ERR_ESPNOW_FULL=0x306C, ESP_ERR_ESPNOW_NO_MEM=0x306D, ESP_ERR_ESPNOW_EXIST=0x306E");
  }
  
  Serial.println("=== READY TO SEND ===");
  Serial.println("Đảm bảo receiver đã sẵn sàng!");
}

void loop() {
  updateMPU();
  updateDS18B20();
  
  String data = serializeData();
  Serial.print("Sending: ");
  Serial.println(data);
  
  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)data.c_str(), data.length() + 1);
  
  if (result != ESP_OK) {
    Serial.printf("Send Error: %d\n", result);
  }
  
  delay(2000); // Tăng lên 2 giây để dễ debug
}