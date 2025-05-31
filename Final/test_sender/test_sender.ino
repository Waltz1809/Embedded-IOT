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

const int WIFI_CHANNEL = 11;   // Wifi channel. Thực tế test cho thấy channel 1, 6 hay báo lỗi send status.

uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC};

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_MPU6050 mpu;
float accX, accY, accZ;
float temperature = 0.0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gud\n" : "Nah\n");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  esp_err_t channel_set_result = esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (channel_set_result != ESP_OK) {
    Serial.print("ERROR: Setting WiFi channel failed: "); 
    Serial.println(esp_err_to_name(channel_set_result));
    while (true) delay(1000);
  }

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("ERROR: Failed to find MPU6050."); 
    while (true) delay(1000);
  }

  sensors.begin();
  if (sensors.getDeviceCount() == 0) {
    Serial.println("ERROR: No DS18B20 sensors found."); 
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

  if (esp_now_is_peer_exist(receiverMAC)) {
  } else {
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    if (addStatus != ESP_OK) {
      Serial.print("ERROR: Failed to add ESP-NOW peer: "); 
      Serial.println(esp_err_to_name(addStatus));
      while (true) delay(1000);
    }
  }
  esp_now_register_send_cb(OnDataSent); 
}

void loop() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C || temperature == 85.0 || temperature == -127.0) {
    Serial.println("WARNING: Failed to read from DS18B20 or invalid reading."); 
  }

  sensors_event_t a, g, temp_mpu;
  if (mpu.getEvent(&a, &g, &temp_mpu)) {
    accX = a.acceleration.x;
    accY = a.acceleration.y;
    accZ = a.acceleration.z;
  } else {
    Serial.println("WARNING: Failed to read from MPU6050."); 
    accX = 0; accY = 0; accZ = 0;
  }

  char msg[100];
  snprintf(msg, sizeof(msg), "T:%.2f X:%.2f Y:%.2f Z:%.2f", temperature, accX, accY, accZ);

  esp_err_t result = esp_now_send(receiverMAC, (uint8_t *)msg, strlen(msg));

  if (result == ESP_OK) {
    Serial.print("[SENT] "); Serial.println(msg); 
  } else {
    Serial.print("ERROR: Sending data failed: "); 
    Serial.println(esp_err_to_name(result));
  }

  delay(3000);
}
