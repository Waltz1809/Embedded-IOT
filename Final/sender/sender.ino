#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

#define ONE_WIRE_BUS 25
const int I2C_SDA_PIN = 27;
const int I2C_SCL_PIN = 26;

const int WIFI_CHANNEL = 11;
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC}; // MAC CỦA RECEIVER

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_MPU6050 mpu;

float temperature = 0.0;
float shakeMagnitude = 0.0;
float accX_raw, accY_raw, accZ_raw; // Khai báo ở global scope để dễ truy cập

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Giữ lại log này để biết trạng thái gửi
    Serial.print("Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gud" : "Nah");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("ERROR: Setting WiFi channel failed!");
    while (true) delay(1000);
  }

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("ERROR: Failed to find MPU6050.");
    while (true) delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  Serial.println("MPU6050 initialized.");

  sensors.begin();
  if (sensors.getDeviceCount() == 0) {
    Serial.println("ERROR: No DS18B20 sensors found.");
  } else {
    sensors.setResolution(12);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW initialization failed!");
    while (true) delay(1000);
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  if (!esp_now_is_peer_exist(receiverMAC)) {
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    if (addStatus != ESP_OK) {
      Serial.print("ERROR: Failed to add ESP-NOW peer: ");
      Serial.println(esp_err_to_name(addStatus));
      while (true) delay(1000);
    }
  }
  esp_now_register_send_cb(OnDataSent);
  Serial.println("Sender setup complete.");
}

void loop() {
  sensors.requestTemperatures();
  // Đợi một chút cho cảm biến DS18B20 hoàn thành việc đọc nhiệt độ ở độ phân giải 12-bit (tối đa 750ms)
  // Tuy nhiên, vì vòng lặp có delay(3000) nên có thể bỏ qua delay riêng cho DS18B20 ở đây nếu lần đọc trước đã hoàn thành
  delay(100); // Chờ một chút nếu cần, hoặc có thể không cần nếu delay vòng lặp đủ lớn
  temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C || temperature == 85.0 || temperature == -127.0) {
    temperature = -999.0; // Giá trị lỗi
  }

  sensors_event_t a, g, temp_mpu_event;
  if (mpu.getEvent(&a, &g, &temp_mpu_event)) {
    accX_raw = a.acceleration.x;
    accY_raw = a.acceleration.y;
    accZ_raw = a.acceleration.z;
    shakeMagnitude = sqrt(accX_raw * accX_raw + accY_raw * accY_raw + accZ_raw * accZ_raw);
  } else {
    accX_raw = 0; accY_raw = 0; accZ_raw = 0;
    shakeMagnitude = 0.0;
  }

  // In dữ liệu ra Serial theo định dạng yêu cầu
  Serial.print("T:"); Serial.print(temperature, 1);
  Serial.print(", AccX:"); Serial.print(accX_raw, 2);
  Serial.print(", AccY:"); Serial.print(accY_raw, 2);
  Serial.print(", AccZ:"); Serial.print(accZ_raw, 2);
  Serial.print(", S:"); Serial.println(shakeMagnitude, 2); // println ở cuối

  // Chuẩn bị message gửi qua ESP-NOW (chỉ T và S)
  char msg[50]; // Kích thước đủ cho "T:xx.x S:yy.y" và null terminator
  snprintf(msg, sizeof(msg), "T:%.1f S:%.1f", temperature, shakeMagnitude); // Làm tròn đến 1 chữ số thập phân

  esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)msg, strlen(msg));
  // Callback OnDataSent sẽ in trạng thái gửi

  delay(3000); // Gửi dữ liệu mỗi 3 giây
}
