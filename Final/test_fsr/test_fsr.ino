#define FSR_PIN 34 // Chọn chân analog bất kỳ của ESP32 (34, 35, 32...)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Đang test FSR402...");
}

void loop() {
  int fsrValue = analogRead(FSR_PIN); // ESP32: giá trị 0–4095 (12-bit)
  Serial.print("Giá trị FSR: ");
  Serial.println(fsrValue);
  delay(500);
}
