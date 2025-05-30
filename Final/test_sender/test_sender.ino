#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#define ONE_WIRE_BUS 25       // DS18B20
const int I2C_SDA_PIN = 27;   // SDA - MPU6050
const int I2C_SCL_PIN = 26;   // SCL - MPU6050

const int WIFI_CHANNEL = 6;   // Wifi channel

uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC};

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_MPU6050 mpu;
float accX, accY, accZ;
float temperature = 0.0;

// Hàm callback khi dữ liệu ESP-NOW được gửi
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP-NOW Sender Initializing...");
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

  // MPU6050
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!mpu.begin(0x68, &Wire)) { // 0x68 là địa chỉ I2C mặc định của MPU6050
    Serial.println("Failed to find MPU6050 chip. Check connections.");
    while (true) delay(1000); 
  }
  Serial.println("MPU6050 Found!");

  // Dallas Temperature (DS18B20)
  sensors.begin();
  if (sensors.getDeviceCount() == 0) {
    Serial.println("No Dallas temperature sensors found! Check connections.");
  } else {
    Serial.print(sensors.getDeviceCount());
    Serial.println(" Dallas temperature sensors found.");
  }

  // 3. Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed!");
    while (true) delay(1000); 
  }
  Serial.println("ESP-NOW initialized successfully.");

  esp_now_peer_info_t peerInfo = {}; 
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;  
  peerInfo.encrypt = false;         
  peerInfo.ifidx = WIFI_IF_STA;     

  if (esp_now_is_peer_exist(receiverMAC)) {
      Serial.println("Peer already exists. Not adding again.");
  } else {
      esp_err_t addStatus = esp_now_add_peer(&peerInfo);
      if (addStatus != ESP_OK) {
          Serial.print("Failed to add peer: ");
          Serial.println(esp_err_to_name(addStatus));
          while (true) delay(1000); // Dừng nếu không thêm được peer
      } else {
          Serial.println("Peer added successfully.");
      }
  }
  Serial.println("Sender ready to send data.");
}

void loop() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0); 
  if (temperature == DEVICE_DISCONNECTED_C || temperature == 85.0 || temperature == -127.0) { 
    Serial.println("Warning: Failed to read from DS18B20 sensor or invalid reading.");
  }

  // Đọc gia tốc từ MPU6050
  sensors_event_t a, g, temp_mpu; // temp_mpu là nhiệt độ từ chip MPU6050, không phải DS18B20
  if (mpu.getEvent(&a, &g, &temp_mpu)) {
    accX = a.acceleration.x;
    accY = a.acceleration.y;
    accZ = a.acceleration.z;
  } else {
    Serial.println("Warning: Failed to read from MPU6050 sensor.");
    // Gán giá trị mặc định nếu đọc lỗi
    accX = 0; accY = 0; accZ = 0;
  }

  char msg[100]; 
  snprintf(msg, sizeof(msg), "T:%.2f X:%.2f Y:%.2f Z:%.2f", temperature, accX, accY, accZ);

  esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)msg, strlen(msg));

  if (result == ESP_OK) {
    Serial.print("Sent: "); Serial.println(msg);
  } else {
    Serial.print("Error sending data: ");
    Serial.println(esp_err_to_name(result));
  }

  delay(3000); // Thời gian gửi
}
