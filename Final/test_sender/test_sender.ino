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

// --- THAY ĐỔI NGƯỠNG GAS Ở ĐÂY ---
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

// --- CẬP NHẬT LOGIC KIỂM TRA NGƯỠNG GAS ---
void checkAndTriggerLocalSenderAlerts() {
  bool alertState = false;
  if (currentSensorData.temp_drill >= SENDER_THRESHOLDS.temp_drill_critical ||
      currentSensorData.temp_center >= SENDER_THRESHOLDS.temp_center_critical ||
      currentSensorData.temp_mine >= SENDER_THRESHOLDS.temp_mine_critical ||
      currentSensorData.vibration >= SENDER_THRESHOLDS.vibration_severe ||
      (currentSensorData.gas_mq2 >= SENDER_THRESHOLDS.gas_mq2_severe && currentSensorData.gas_mq2 != -999) ||
      (currentSensorData.gas_mq5 >= SENDER_THRESHOLDS.gas_mq5_severe && currentSensorData.gas_mq5 != -999) ||
      (currentSensorData.gas_mq9 >= SENDER_THRESHOLDS.gas_mq9_severe && currentSensorData.gas_mq9 != -999) || // Vẫn kiểm tra MQ9
      (currentSensorData.gas_mq135 >= SENDER_THRESHOLDS.gas_mq135_severe && currentSensorData.gas_mq135 != -999)) {
    alertState = true;
  }
  digitalWrite(SENDER_BUZZER_PIN, alertState);
  digitalWrite(SENDER_STATUS_LED_PIN, alertState);
}
// --- KẾT THÚC CẬP NHẬT LOGIC KIỂM TRA NGƯỠNG GAS ---

void printSensorDataToSend(const SensorData &data) {
  Serial.print("SND: ");
  Serial.print("Drill:"); Serial.print(data.temp_drill, 1);
  Serial.print("C Cntr(DHT11):"); Serial.print(data.temp_center, 1);
  Serial.print("C Mine(DHT22):"); Serial.print(data.temp_mine, 1);
  Serial.print("C Vib:"); Serial.print(data.vibration, 1);
  Serial.print(" MQ2:"); Serial.print(data.gas_mq2);
  Serial.print(" MQ5:"); Serial.print(data.gas_mq5);
  Serial.print(" MQ9:"); Serial.print(data.gas_mq9);
  Serial.print(" MQ135:"); Serial.print(data.gas_mq135);
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  ds18b20.begin();
  dht11_center.begin();
  dht22_mine.begin();

  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("Không tìm thấy MPU6050!");
    // currentSensorData.vibration = -999.0; // Có thể gán giá trị lỗi ở đây nếu muốn
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // Thêm bộ lọc nếu cần
  }
  
  pinMode(MQ2_PIN, INPUT);
  pinMode(MQ5_PIN, INPUT);
  pinMode(MQ9_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  pinMode(SENDER_BUZZER_PIN, OUTPUT);
  pinMode(SENDER_STATUS_LED_PIN, OUTPUT);
  digitalWrite(SENDER_BUZZER_PIN, LOW);
  digitalWrite(SENDER_STATUS_LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi khởi tạo ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Lỗi thêm peer");
    return;
  }
}

void loop() {
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    ds18b20.requestTemperatures();
    currentSensorData.temp_drill = ds18b20.getTempCByIndex(0);
    if (currentSensorData.temp_drill == DEVICE_DISCONNECTED_C || currentSensorData.temp_drill == 85.0) {
        currentSensorData.temp_drill = -999.0;
    }

    currentSensorData.temp_center = dht11_center.readTemperature();
    if (isnan(currentSensorData.temp_center)) {
        currentSensorData.temp_center = -999.0;
    }

    currentSensorData.temp_mine = dht22_mine.readTemperature();
    if (isnan(currentSensorData.temp_mine)) {
        currentSensorData.temp_mine = -999.0;
    }

    sensors_event_t a, g, temp_mpu;
    if (mpu.getEvent(&a, &g, &temp_mpu)) {
      currentSensorData.vibration = sqrt(a.acceleration.x * a.acceleration.x +
                                        a.acceleration.y * a.acceleration.y +
                                        a.acceleration.z * a.acceleration.z);
    } else {
      currentSensorData.vibration = -999.0;
    }

    currentSensorData.gas_mq2 = analogRead(MQ2_PIN);
    currentSensorData.gas_mq5 = analogRead(MQ5_PIN);
    // Nếu MQ9 vẫn bị lỗi, bạn có thể gán cứng giá trị lỗi:
    // currentSensorData.gas_mq9 = -999; 
    // Hoặc đọc bình thường nếu nó đã được sửa:
    currentSensorData.gas_mq9 = analogRead(MQ9_PIN); 
    currentSensorData.gas_mq135 = analogRead(MQ135_PIN);
    
    // Xử lý giá trị đọc ADC có thể không ổn định (ví dụ đọc ra 0 hoặc 4095 bất thường)
    if (currentSensorData.gas_mq2 == 0 || currentSensorData.gas_mq2 == 4095) currentSensorData.gas_mq2 = -999;
    if (currentSensorData.gas_mq5 == 0 || currentSensorData.gas_mq5 == 4095) currentSensorData.gas_mq5 = -999;
    if (currentSensorData.gas_mq9 == 0 || currentSensorData.gas_mq9 == 4095) currentSensorData.gas_mq9 = -999; // Áp dụng cho MQ9
    if (currentSensorData.gas_mq135 == 0 || currentSensorData.gas_mq135 == 4095) currentSensorData.gas_mq135 = -999;


    checkAndTriggerLocalSenderAlerts();
    printSensorDataToSend(currentSensorData);
    esp_now_send(receiverMAC, (uint8_t *) &currentSensorData, sizeof(currentSensorData));
  }
}
