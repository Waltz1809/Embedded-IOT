#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

const int N = 10; // số lần lấy mẫu để trung bình
float avgAX = 0, avgAY = 0, avgAZ = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(27, 26); // SDA, SCL

  Serial.println("Đang khởi động MPU6050...");
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("Không tìm thấy MPU6050.");
    while (1);
  }

  Serial.println("MPU6050 đã kết nối.");
}

void loop() {
  long sumAX = 0, sumAY = 0, sumAZ = 0;

  // Lọc trung bình đơn giản trên 10 lần đọc
  for (int i = 0; i < N; i++) {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    sumAX += ax;
    sumAY += ay;
    sumAZ += az;
    delay(5); // ngắn, tránh làm chậm loop nhiều
  }

  avgAX = (float)sumAX / N;
  avgAY = (float)sumAY / N;
  avgAZ = (float)sumAZ / N;

  // Chuẩn hóa về đơn vị g (1g = 16384 ở thang ±2g)
  float axg = avgAX / 16384.0;
  float ayg = avgAY / 16384.0;
  float azg = avgAZ / 16384.0;

  // Tính góc nghiêng
  float angleX = atan2(ayg, azg) * 180 / PI;
  float angleY = atan2(-axg, sqrt(ayg * ayg + azg * azg)) * 180 / PI;

  // In kết quả
  Serial.println("-----------");
  Serial.print("Gia tốc (g): X="); Serial.print(axg, 3);
  Serial.print(" | Y="); Serial.print(ayg, 3);
  Serial.print(" | Z="); Serial.println(azg, 3);

  Serial.print("Góc nghiêng: X="); Serial.print(angleX, 2);
  Serial.print("°, Y="); Serial.print(angleY, 2);
  Serial.println("°");
  Serial.println("-----------\n");

  delay(1000); // in mỗi 1 giây
}
