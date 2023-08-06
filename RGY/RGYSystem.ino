const byte RED_LIGHT_PIN = 9;
const byte YELLOW_LIGHT_PIN = 10;
const byte GREEN_LIGHT_PIN = 11;

const byte NUM_LEDS = 4; // số lượng LED 7 đoạn sử dụng
const byte LED_PIN[] = {5, 4, 3, 2}; // chân kết nối các LED 7 đoạn
byte digitPattern[] = {
  0b11111100, // 0
  0b01100000, // 1
  0b11011010, // 2
  0b11110010, // 3
  0b01100110, // 4
  0b10110110, // 5
  0b10111110, // 6
  0b11100000, // 7
  0b10010000, // 8 (with dash)
  0b10010000, // 9
  0b00000010, // dot
  0b00000000, // blank
};
unsigned long timer = 0; // đếm thời gian để đổi đèn
byte currentMode = 0; // hiện tại đang ở chế độ nào

void setup() {
  pinMode(RED_LIGHT_PIN, OUTPUT);
  pinMode(YELLOW_LIGHT_PIN, OUTPUT);
  pinMode(GREEN_LIGHT_PIN, OUTPUT);
  
  for (byte i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PIN[i], OUTPUT);
  }
  Serial.begin(9600); // khai báo cho Serial có tốc độ truyền dữ liệu là 9600 baud
}

void loop() {
  unsigned long elapsedTime = millis() - timer;
  
  switch (currentMode) {
    case 0: // đèn đỏ, 5s
      if (elapsedTime <= 5000) {
        updateDisplay((5000 - elapsedTime) / 1000); // hiển thị thời gian đếm ngược
        digitalWrite(RED_LIGHT_PIN, HIGH); // đèn đỏ sáng
        digitalWrite(YELLOW_LIGHT_PIN, LOW); // đèn vàng tắt
        digitalWrite(GREEN_LIGHT_PIN, LOW); // đèn xanh tắt
      } else {
        timer = millis();
        currentMode = 1; // chuyển qua chế độ đèn xanh
      }
      break;
    case 1: // đèn xanh, 6s
      if (elapsedTime <= 6000) {
        updateDisplay((6000 - elapsedTime) / 1000); // hiển thị thời gian đếm ngược
        digitalWrite(RED_LIGHT_PIN, LOW); // đèn đỏ tắt
        digitalWrite(YELLOW_LIGHT_PIN, LOW); // đèn vàng tắt
        digitalWrite(GREEN_LIGHT_PIN, HIGH); // đèn xanh sáng
      } else {
        timer = millis();
        currentMode = 2; // chuyển qua chế độ đèn vàng
      }
      break;
    case 2: // đèn vàng, 3s
      if (elapsedTime <= 3000) {
        updateDisplay((3000 - elapsedTime) / 1000); // hiển thị thời gian đếm ngược
        digitalWrite(RED_LIGHT_PIN, LOW); // đèn đỏ tắt
        digitalWrite(YELLOW_LIGHT_PIN, HIGH); // đèn vàng sáng
        digitalWrite(GREEN_LIGHT_PIN, LOW); // đèn xanh tắt
      } else {
        timer = millis();
        currentMode = 0; // chuyển qua chế độ đèn đỏ
      }
      break;
    default: // trường hợp này không nên xảy ra
      currentMode = 0;
      break;
  }
}

void updateDisplay(byte val) {
  for (byte i = 0; i < NUM_LEDS; i++) {
    byte digit = val % 10;
    val /= 10;
    digitalWrite(LED_PIN[i], HIGH);
    shiftOut(LED_PIN[i], 2, MSBFIRST, digitPattern[digit]);
    digitalWrite(LED_PIN[i], LOW);
    if (val == 0 && digit == 0) {
      break; // kết thúc việc cập nhật hiển thị LED 7 đoạn
    }
  }
  delay(1); // nhấp nháy LED 7 đoạn
}
