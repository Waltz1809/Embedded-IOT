#include <Servo.h> 
#define Servo_PWM 8 
Servo MG995_Servo;
int led = 7;
int heatSensor = A1;
void setup() {
  pinMode(led,OUTPUT);
  pinMode(3, OUTPUT); 
  pinMode(A0, INPUT);
  MG995_Servo.attach(Servo_PWM);
  Serial.begin(9600);
}

int checkFire(int x, int y){
  if(x<=y){
    return 1;
  } else {
    return 0;  
  }
}

void alarm(int sensor1,int sensor2,int sensor3){
  if (checkFire(sensor1, 300)==1){
    tone(3, 2000);
    digitalWrite(led, HIGH);
    MG995_Servo.write(0); 
    delay(3000);
    MG995_Servo.write(90);
    MG995_Servo.attach(Servo_PWM);
    Serial.println("Có cháy");
    delay(5000);
  } 
  else if (checkFire(sensor2,85)==1){
    tone(3, 2000);
    digitalWrite(led, HIGH);
    MG995_Servo.write(0); 
    delay(3000);
    MG995_Servo.write(90);
    MG995_Servo.attach(Servo_PWM);
    Serial.println("Có cháy");
    delay(5000);
  }
  else if (checkFire(400, sensor3)==1){
    tone(3, 2000); 
    digitalWrite(led, HIGH);
    MG995_Servo.write(0); 
    delay(3000);
    MG995_Servo.write(90);
    MG995_Servo.attach(Servo_PWM);
    Serial.println("Có cháy");
    delay(5000);
  } 
  else {
    digitalWrite(led, LOW);
  }
}

void loop() {
  int lightSensor = analogRead(A0);   //Đọc Analog chân A0 - cảm biến ánh sáng
  int heatSensor = analogRead(A1);   //Đọc Analog chân A1 - cảm biến MQ-2
  int gasSensor = analogRead(A2);   //đọc giá trị điện áp ở chân A2 - nhiệt trở LM35. 
  delay(1000);            
  alarm(lightSensor,heatSensor,gasSensor);
  Serial.print("Anh Sang:");
  Serial.println(lightSensor);
  Serial.print("Nhiet Do:");
  Serial.println(heatSensor);
  Serial.print("Gas:");
  Serial.println(gasSensor);
  delay(2000);
}
