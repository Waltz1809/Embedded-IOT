#define BLYNK_TEMPLATE_ID "TMPL62mlqkC2K"
#define BLYNK_TEMPLATE_NAME "Waltz32"
#define BLYNK_AUTH_TOKEN "V1tBXpddh5VDEIKxYpA4Vq-AEbpg0QTH"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Servo.h>

#define BLYNK_PRINT Serial

#define DHT_PIN 23       
#define DHTTYPE DHT11    
#define PIR_PIN 34       
#define MQ2_PIN 33      
#define BUZZER_PIN 25   
#define LED_PIN 26    
#define SERVO_PIN 15    

#define VPIN_DHT_PHONG V0
#define VPIN_MQ2 V1
#define VPIN_DHT V2
#define VPIN_PIR V3

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "NHA TRANG UNIVERSITY";       
char pass[] = "wifintu!";   
DHT dht(DHT_PIN, DHTTYPE);
Servo myservo;


int ANGLE_OPEN = 90;   
int ANGLE_CLOSE = 180;    
unsigned long PREVIOUS_MILLIS = 0; 
bool FIRE_DETECTED = false;

BLYNK_CONNECTED()
{
 Blynk.syncAll();
}


void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  myservo.attach(SERVO_PIN);
  myservo.write(ANGLE_CLOSE); 
  Blynk.begin(auth, ssid, pass); 
}

void loop() {
  Blynk.run();
  unsigned long CURRENT_MILLIS = millis(); 

  if (CURRENT_MILLIS - PREVIOUS_MILLIS >= 2000) { 
    PREVIOUS_MILLIS = CURRENT_MILLIS;

    float TEMPERATURE = dht.readTemperature();

    int GAS_VALUE = analogRead(MQ2_PIN);

    int PIR_VALUE = digitalRead(PIR_PIN);

    if (GAS_VALUE >= 800 || TEMPERATURE >= 40) { 
      myservo.write(ANGLE_OPEN);
      digitalWrite(BUZZER_PIN, HIGH);
      Blynk.virtualWrite(VPIN_MQ2, HIGH);
      Serial.println("Nghi vấn có cháy");
      Blynk.logEvent("FIRE_DETECTED"); 
      FIRE_DETECTED = true;
    } else {
      myservo.write(ANGLE_CLOSE);
      Blynk.virtualWrite(VPIN_MQ2, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      FIRE_DETECTED = false;
    }

    if (FIRE_DETECTED == true) {
      if (PIR_VALUE == HIGH) { 
      digitalWrite(LED_PIN, HIGH);
      Blynk.virtualWrite(VPIN_PIR, HIGH);
      Blynk.logEvent("MOTION_DETECTED");
      Serial.println("Vẫn còn người trong khu vực"); 
      } else {
      Blynk.virtualWrite(VPIN_PIR, LOW);
      digitalWrite(LED_PIN, LOW);
      }
    }

    Blynk.virtualWrite(VPIN_DHT, TEMPERATURE);
    
    Serial.print("Nhiet Do: ");
    Serial.print(TEMPERATURE);
    Serial.print(" C, Gas: ");
    Serial.println(GAS_VALUE);
  }
}
