#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"
#define BLYNK_AUTH_TOKEN "V1tBXpddh5VDEIKxYpA4Vq-AEbpg0QTH"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define BLYNK_PRINT Serial

#define DHT_PIN 23       
#define DHTTYPE DHT11    
#define PIR_PIN 34       
#define MQ4_PIN 33       // Methane sensor (thay MQ2 bằng MQ4 cho khí metan)
#define MQ7_PIN 32       // CO sensor
#define MQ136_PIN 35     // H2S sensor
#define BUZZER_PIN 25   
#define LED_PIN 26    
#define SERVO_PIN 15    
#define WATER_LEVEL_PIN 27  // Cảm biến mực nước

#define VPIN_TEMP V0
#define VPIN_METHANE V1
#define VPIN_CO V2
#define VPIN_H2S V3
#define VPIN_MOTION V4
#define VPIN_VIBRATION V5
#define VPIN_WATER V6

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Phuong Hoa";       
char pass[] = "65dienbienphu";   
DHT dht(DHT_PIN, DHTTYPE);
Servo myservo;
Adafruit_MPU6050 mpu;

int ANGLE_OPEN = 90;   
int ANGLE_CLOSE = 180;    
unsigned long PREVIOUS_MILLIS = 0; 
bool DANGER_DETECTED = false;

// Ngưỡng cảnh báo cho môi trường hầm mỏ
#define METHANE_THRESHOLD 500   // Ngưỡng khí metan (ppm)
#define CO_THRESHOLD 50         // Ngưỡng CO (ppm)
#define H2S_THRESHOLD 20        // Ngưỡng H2S (ppm)
#define TEMP_THRESHOLD 35       // Ngưỡng nhiệt độ (°C)
#define VIBRATION_THRESHOLD 1.5 // Ngưỡng dao động (g)
#define WATER_THRESHOLD 700     // Ngưỡng mực nước

BLYNK_CONNECTED()
{
 Blynk.syncAll();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  dht.begin();
  
  // Khởi tạo cảm biến gia tốc MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  pinMode(PIR_PIN, INPUT);
  pinMode(MQ4_PIN, INPUT);
  pinMode(MQ7_PIN, INPUT);
  pinMode(MQ136_PIN, INPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  myservo.attach(SERVO_PIN);
  myservo.write(ANGLE_CLOSE); 
  Blynk.begin(auth, ssid, pass); 
  
  Serial.println("Hệ thống giám sát an toàn hầm mỏ đã sẵn sàng");
}

void loop() {
  Blynk.run();
  unsigned long CURRENT_MILLIS = millis(); 

  if (CURRENT_MILLIS - PREVIOUS_MILLIS >= 2000) { 
    PREVIOUS_MILLIS = CURRENT_MILLIS;

    // Đọc các giá trị cảm biến
    float TEMPERATURE = dht.readTemperature();
    int METHANE_VALUE = analogRead(MQ4_PIN);
    int CO_VALUE = analogRead(MQ7_PIN);
    int H2S_VALUE = analogRead(MQ136_PIN);
    int WATER_LEVEL = analogRead(WATER_LEVEL_PIN);
    int PIR_VALUE = digitalRead(PIR_PIN);
    
    // Đọc giá trị dao động từ MPU6050
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float VIBRATION = sqrt(a.acceleration.x * a.acceleration.x + 
                          a.acceleration.y * a.acceleration.y + 
                          a.acceleration.z * a.acceleration.z);

    // Gửi dữ liệu lên Blynk
    Blynk.virtualWrite(VPIN_TEMP, TEMPERATURE);
    Blynk.virtualWrite(VPIN_METHANE, METHANE_VALUE);
    Blynk.virtualWrite(VPIN_CO, CO_VALUE);
    Blynk.virtualWrite(VPIN_H2S, H2S_VALUE);
    Blynk.virtualWrite(VPIN_VIBRATION, VIBRATION);
    Blynk.virtualWrite(VPIN_WATER, WATER_LEVEL);
    
    // Kiểm tra các điều kiện nguy hiểm
    if (METHANE_VALUE >= METHANE_THRESHOLD || 
        CO_VALUE >= CO_THRESHOLD || 
        H2S_VALUE >= H2S_THRESHOLD || 
        TEMPERATURE >= TEMP_THRESHOLD ||
        VIBRATION >= VIBRATION_THRESHOLD ||
        WATER_LEVEL >= WATER_THRESHOLD) {
      
      // Kích hoạt thiết bị cảnh báo
      myservo.write(ANGLE_OPEN);
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      
      // Gửi cảnh báo lên Blynk
      if (METHANE_VALUE >= METHANE_THRESHOLD) {
        Blynk.logEvent("METHANE_WARNING", "Nồng độ khí Methane vượt ngưỡng!");
        Serial.println("CẢNH BÁO: Nồng độ khí Methane cao!");
      }
      
      if (CO_VALUE >= CO_THRESHOLD) {
        Blynk.logEvent("CO_WARNING", "Nồng độ khí CO vượt ngưỡng!");
        Serial.println("CẢNH BÁO: Nồng độ khí CO cao!");
      }
      
      if (H2S_VALUE >= H2S_THRESHOLD) {
        Blynk.logEvent("H2S_WARNING", "Nồng độ khí H2S vượt ngưỡng!");
        Serial.println("CẢNH BÁO: Nồng độ khí H2S cao!");
      }
      
      if (VIBRATION >= VIBRATION_THRESHOLD) {
        Blynk.logEvent("COLLAPSE_WARNING", "Phát hiện rung động bất thường!");
        Serial.println("CẢNH BÁO: Có nguy cơ sập hầm!");
      }
      
      if (WATER_LEVEL >= WATER_THRESHOLD) {
        Blynk.logEvent("FLOOD_WARNING", "Mực nước tăng cao!");
        Serial.println("CẢNH BÁO: Nguy cơ ngập nước!");
      }
      
      DANGER_DETECTED = true;
    } else {
      myservo.write(ANGLE_CLOSE);
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      DANGER_DETECTED = false;
    }

    // Kiểm tra chuyển động người (công nhân) khi có nguy hiểm
    if (DANGER_DETECTED == true) {
      if (PIR_VALUE == HIGH) { 
        Blynk.virtualWrite(VPIN_MOTION, HIGH);
        Blynk.logEvent("WORKER_DETECTED", "Phát hiện công nhân trong khu vực nguy hiểm!");
        Serial.println("CẢNH BÁO: Vẫn còn người trong khu vực nguy hiểm!"); 
      } else {
        Blynk.virtualWrite(VPIN_MOTION, LOW);
      }
    }

    // In thông tin giám sát
    Serial.print("Nhiệt độ: ");
    Serial.print(TEMPERATURE);
    Serial.print("°C | Methane: ");
    Serial.print(METHANE_VALUE);
    Serial.print(" | CO: ");
    Serial.print(CO_VALUE);
    Serial.print(" | H2S: ");
    Serial.print(H2S_VALUE);
    Serial.print(" | Độ rung: ");
    Serial.print(VIBRATION);
    Serial.print("g | Mực nước: ");
    Serial.println(WATER_LEVEL);
  }
}
