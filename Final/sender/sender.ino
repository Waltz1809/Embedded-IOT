#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <math.h>

#define DS18B20_PIN 25
#define DHT11_CENTER_PIN 17
#define DHT22_MINE_PIN   22
#define I2C_SDA_PIN 27
#define I2C_SCL_PIN 26

#define MQ2_PIN   32
#define MQ5_PIN   33
#define MQ9_PIN   14
#define MQ135_PIN 35

#define SENDER_BUZZER_PIN 15
#define SENDER_STATUS_LED_PIN 12

const int WIFI_CHANNEL = 11;
uint8_t receiverMAC[] = {0xC8, 0xF0, 0x9E, 0x50, 0x3E, 0xEC};

OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
DHT dht11_center(DHT11_CENTER_PIN, DHT11);
DHT dht22_mine(DHT22_MINE_PIN, DHT22);
Adafruit_MPU6050 mpu;

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
SensorData currentSensorData;
unsigned long lastSendTime = 0;
const long sendInterval = 1000;

const struct SenderAlertThresholds {
  float temp_drill_critical = 70.0;
  float temp_center_critical = 40.0;
  float temp_mine_critical = 40.0;
  float vibration_severe = 18.0;
  int gas_mq2_severe = 800;     // Ngưỡng cho MQ-2 (nền 300-400, báo động 600-700)
  int gas_mq5_severe = 1000;    // Ngưỡng cho MQ-5 (nền 600-700, báo động >1000)
  int gas_mq9_severe = 1500;    // Ngưỡng tạm thời cho MQ-9 (điều chỉnh nếu cần)
  int gas_mq135_severe = 1500;  // Ngưỡng cho MQ-135 (nền 1000, báo động >2000)
} SENDER_THRESHOLDS;
// --- KẾT THÚC THAY ĐỔI NGƯỠNG GAS ---

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gud" : "Nah");
}