#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing" 
#define BLYNK_AUTH_TOKEN "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"

#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h> 

char ssid[] = "Phuong Hoa";
char pass[] = "65dienbienphu";

const int LED_PIN = 2; 
const int WIFI_CHANNEL = 6; 

char latestMsgBuffer[251]; 
volatile bool newDataAvailable = false; 
portMUX_TYPE onReceiveMux = portMUX_INITIALIZER_UNLOCKED;

const float TEMP_THRESHOLD_HIGH = 50.0; 
const float TEMP_HYSTERESIS = 2.0;      
bool highTempAlertActive = false;       
unsigned long lastHighTempAlertTime = 0; 

const float SHAKE_MAGNITUDE_THRESHOLD = 2.5; 
const float SHAKE_HYSTERESIS = 0.5;          
bool shakeAlertActive = false;              
unsigned long lastShakeAlertTime = 0; 

const unsigned long NOTIFICATION_COOLDOWN = 5000; 

// event code
const char* EVENT_CODE_HIGH_TEMP = "high_temp_alert";
const char* EVENT_CODE_SHAKE_ALERT = "shake_alert";
const char* EVENT_CODE_TEMP_NORMAL = "temp_normal";     
const char* EVENT_CODE_SHAKE_STOPPED = "shake_stopped"; 

// Callback function khi nhận được dữ liệu ESP-NOW
// Sử dụng portENTER_CRITICAL/portEXIT_CRITICAL vì onReceive chạy trong context của WiFi task
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  portENTER_CRITICAL(&onReceiveMux);
  int copyLen = (len >= sizeof(latestMsgBuffer)) ? (sizeof(latestMsgBuffer) - 1) : len;
  memcpy(latestMsgBuffer, incomingData, copyLen);
  latestMsgBuffer[copyLen] = '\0'; 
  newDataAvailable = true;
  portEXIT_CRITICAL(&onReceiveMux);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  memset(latestMsgBuffer, 0, sizeof(latestMsgBuffer));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  delay(100);     

  Serial.print("Setting WiFi channel to: "); Serial.println(WIFI_CHANNEL);
  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("Error setting WiFi channel!"); while (true) delay(1000);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed!"); while (true) delay(1000);
  }

  if (esp_now_register_recv_cb(onDataRecv) != ESP_OK) { // Đổi tên hàm callback cho rõ ràng
    Serial.println("Failed to register ESP-NOW receive callback!"); while (true) delay(1000);
  }

  Serial.print("Connecting to WiFi: "); Serial.println(ssid);
  WiFi.begin(ssid, pass); 

  int connect_tries = 0;
  const int max_connect_tries = 20;
  while (WiFi.status() != WL_CONNECTED && connect_tries < max_connect_tries) {
    delay(500); Serial.print("."); connect_tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Blynk.config(BLYNK_AUTH_TOKEN); 
    Blynk.connect();
  } else {
    Serial.println("\nFailed to connect to WiFi. Blynk will not be available.");
  }
}

void processReceivedData() {
  char localMsgCopy[sizeof(latestMsgBuffer)];
  bool hasDataToProcess = false;
  portENTER_CRITICAL(&onReceiveMux);
  if (newDataAvailable) {
    memcpy(localMsgCopy, latestMsgBuffer, sizeof(latestMsgBuffer));
    newDataAvailable = false; 
    hasDataToProcess = true;
  }
  portEXIT_CRITICAL(&onReceiveMux);

  if (hasDataToProcess) {
    // In ra thông tin nhận được theo format yêu cầu
    Serial.print("[RECV] Length: "); Serial.print(strlen(localMsgCopy));
    Serial.print(", Data: "); Serial.println(localMsgCopy);
    
    digitalWrite(LED_PIN, HIGH); delay(20); digitalWrite(LED_PIN, LOW);

    float temp = -999.0, accX = 0.0, accY = 0.0, accZ = 0.0;
    int parsedItems = sscanf(localMsgCopy, "T:%f X:%f Y:%f Z:%f", &temp, &accX, &accY, &accZ);

    if (parsedItems == 4) { 
      if (Blynk.connected()) {
        Blynk.virtualWrite(V0, localMsgCopy);
        // 1. Kiểm tra nhiệt độ cao
        if (temp > TEMP_THRESHOLD_HIGH) {
          if (!highTempAlertActive || (millis() - lastHighTempAlertTime > NOTIFICATION_COOLDOWN)) {
            String msg = "Nhiet do ham mo: " + String(temp, 2) + "°C. NGUY HIEM!";
            Blynk.logEvent(EVENT_CODE_HIGH_TEMP, msg); 
            highTempAlertActive = true; 
            lastHighTempAlertTime = millis(); 
          }
        } else if (highTempAlertActive && temp < (TEMP_THRESHOLD_HIGH - TEMP_HYSTERESIS)) {
           String msg = "Nhiet do ham mo da on dinh: " + String(temp, 2) + "°C.";
           Blynk.logEvent(EVENT_CODE_TEMP_NORMAL, msg); 
           highTempAlertActive = false; 
        }

        // 2. Kiểm tra rung lắc mạnh
        float magnitude = sqrt(accX * accX + accY * accY + accZ * accZ);
        if (magnitude > SHAKE_MAGNITUDE_THRESHOLD) {
          if (!shakeAlertActive || (millis() - lastShakeAlertTime > NOTIFICATION_COOLDOWN)) {
            String msg = "Phat hien RUNG LAC MANH! Gia toc: " + String(magnitude, 2) + "G.";
            Blynk.logEvent(EVENT_CODE_SHAKE_ALERT, msg);
            shakeAlertActive = true;
            lastShakeAlertTime = millis();
          }
        } else if (shakeAlertActive && magnitude < (SHAKE_MAGNITUDE_THRESHOLD - SHAKE_HYSTERESIS)) {
           String msg = "Rung lac da dung. Gia toc: " + String(magnitude, 2) + "G.";
           Blynk.logEvent(EVENT_CODE_SHAKE_STOPPED, msg); 
           shakeAlertActive = false;
        }
      } 
    } else {
    }
  } 
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run(); 
  } else {
  }
  processReceivedData(); 
}

