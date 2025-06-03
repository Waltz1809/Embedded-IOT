#define BLYNK_TEMPLATE_ID "TMPL6A2GM6EQp"
#define BLYNK_TEMPLATE_NAME "Testing"
#define BLYNK_AUTH_TOKEN "be91nsWmaRd5za1NwpAKv2ZC4zKhDHX7"

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>

char ssid[] = "Phuong Hoa";
char pass[] = "65dienbienphu";
const int WIFI_CHANNEL = 11;

#define RECEIVER_ALARM_LED_PIN 26
#define RECEIVER_ALARM_BUZZER_PIN 25

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

const struct ReceiverAlertThresholds {
  float temp_drill_critical = 70.0;
  float temp_center_critical = 40.0;
  float temp_mine_critical = 40.0;
  float vibration_severe = 18.0;
  int gas_mq2_severe = 800;
  int gas_mq5_severe = 1000;
  int gas_mq9_severe = 1500;
  int gas_mq135_severe = 1500;
} RCV_THRESHOLDS;

bool blynkAlertActive = false;
unsigned long lastBlynkAlertTime = 0;
const unsigned long BLYNK_NOTIFICATION_COOLDOWN = 3000; // 10 giây

#define VPIN_TEMP_DRILL    V1
#define VPIN_TEMP_CENTER   V2
#define VPIN_TEMP_MINE     V3
#define VPIN_VIBRATION     V4
#define VPIN_GAS_MQ2       V5
#define VPIN_GAS_MQ5       V6
#define VPIN_GAS_MQ9       V7
#define VPIN_GAS_MQ135     V8
#define VPIN_TERMINAL_MSG  V0

void printReceivedData(const SensorData &data) {
  Serial.print("RCV: ");
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

void handleReceiverAlertsAndBlynk(const SensorData &data) {
  bool localAlertState = false;
  String alertMessage = "";

  if (data.temp_drill >= RCV_THRESHOLDS.temp_drill_critical && data.temp_drill != -999.0) {
    alertMessage += "Mũi khoan nóng (" + String(data.temp_drill, 1) + "C)! ";
    localAlertState = true;
  }

  if (data.temp_center >= RCV_THRESHOLDS.temp_center_critical && data.temp_center != -999.0) {
    alertMessage += "Trung tâm(DHT11) nóng (" + String(data.temp_center, 1) + "C)! ";
    localAlertState = true;
  }

  if (data.temp_mine >= RCV_THRESHOLDS.temp_mine_critical && data.temp_mine != -999.0) {
    alertMessage += "Khu mỏ(DHT22) nóng (" + String(data.temp_mine, 1) + "C)! ";
    localAlertState = true;
  }

  if (data.vibration >= RCV_THRESHOLDS.vibration_severe && data.vibration != -999.0) {
    alertMessage += "Rung mạnh (" + String(data.vibration, 1) + ")! ";
    localAlertState = true;
  }

  String gasSpecificAlerts = "";
  bool anyGasAlert = false;

  if (data.gas_mq2 >= RCV_THRESHOLDS.gas_mq2_severe && data.gas_mq2 != -999) {
    gasSpecificAlerts += "MQ2:" + String(data.gas_mq2) + " ";
    anyGasAlert = true;
  }
  if (data.gas_mq5 >= RCV_THRESHOLDS.gas_mq5_severe && data.gas_mq5 != -999) {
    gasSpecificAlerts += "MQ5:" + String(data.gas_mq5) + " ";
    anyGasAlert = true;
  }
  if (data.gas_mq9 >= RCV_THRESHOLDS.gas_mq9_severe && data.gas_mq9 != -999) {
    gasSpecificAlerts += "MQ9:" + String(data.gas_mq9) + " ";
    anyGasAlert = true;
  }
  if (data.gas_mq135 >= RCV_THRESHOLDS.gas_mq135_severe && data.gas_mq135 != -999) {
    gasSpecificAlerts += "MQ135:" + String(data.gas_mq135) + " ";
    anyGasAlert = true;
  }

  if (anyGasAlert) {
    gasSpecificAlerts.trim(); // SỬA LỖI: Trim chuỗi trước
    alertMessage += "Nguy cơ khí độc! (" + gasSpecificAlerts + ") "; // Sau đó sử dụng chuỗi đã trim
    localAlertState = true;
  }

  digitalWrite(RECEIVER_ALARM_LED_PIN, localAlertState);
  digitalWrite(RECEIVER_ALARM_BUZZER_PIN, localAlertState);

  unsigned long currentTime = millis();
  if (localAlertState) {
    if (!blynkAlertActive || (currentTime - lastBlynkAlertTime > BLYNK_NOTIFICATION_COOLDOWN)) {
      Serial.println("BLYNK ALERT: " + alertMessage);
      Blynk.logEvent("critical_alert", alertMessage.substring(0, min((int)alertMessage.length(), 250)));
      Blynk.virtualWrite(VPIN_TERMINAL_MSG, alertMessage);
      lastBlynkAlertTime = currentTime;
      blynkAlertActive = true;
    }
  } else {
    if (blynkAlertActive) {
      Blynk.virtualWrite(VPIN_TERMINAL_MSG, "Hệ thống ổn định.");
    }
    blynkAlertActive = false;
  }
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(SensorData)) {
    SensorData receivedLocalData;
    memcpy(&receivedLocalData, incomingData, sizeof(receivedLocalData));
    printReceivedData(receivedLocalData);

    Blynk.virtualWrite(VPIN_TEMP_DRILL, receivedLocalData.temp_drill);
    Blynk.virtualWrite(VPIN_TEMP_CENTER, receivedLocalData.temp_center);
    Blynk.virtualWrite(VPIN_TEMP_MINE, receivedLocalData.temp_mine);
    Blynk.virtualWrite(VPIN_VIBRATION, receivedLocalData.vibration);
    Blynk.virtualWrite(VPIN_GAS_MQ2, receivedLocalData.gas_mq2);
    Blynk.virtualWrite(VPIN_GAS_MQ5, receivedLocalData.gas_mq5);
    Blynk.virtualWrite(VPIN_GAS_MQ9, receivedLocalData.gas_mq9);
    Blynk.virtualWrite(VPIN_GAS_MQ135, receivedLocalData.gas_mq135);

    handleReceiverAlertsAndBlynk(receivedLocalData);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RECEIVER_ALARM_LED_PIN, OUTPUT);
  pinMode(RECEIVER_ALARM_BUZZER_PIN, OUTPUT);
  digitalWrite(RECEIVER_ALARM_LED_PIN, LOW);
  digitalWrite(RECEIVER_ALARM_BUZZER_PIN, LOW);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi khởi tạo ESP-NOW!");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
  WiFi.begin(ssid, pass);
  delay(5000);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect(5000);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
}
