#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== TEST MPU6050 ===");
  
  // Test cấu hình I2C khác nhau
  Serial.println("Thử cấu hình 1: SDA=21, SCL=22 (default ESP32)");
  Wire.begin(21, 22);
  delay(100);
  
  if (mpu.begin()) {
    Serial.println("✓ MPU6050 hoạt động với SDA=21, SCL=22");
    setupMPU();
    return;
  }
  
  Serial.println("✗ Thất bại với SDA=21, SCL=22");
  Wire.end();
  
  Serial.println("Thử cấu hình 2: SDA=27, SCL=26");
  Wire.begin(27, 26);
  delay(100);
  
  if (mpu.begin()) {
    Serial.println("✓ MPU6050 hoạt động với SDA=27, SCL=26");
    setupMPU();
    return;
  }
  
  Serial.println("✗ Thất bại với SDA=27, SCL=26");
  Wire.end();
  
  Serial.println("Thử cấu hình 3: SDA=4, SCL=5");
  Wire.begin(4, 5);
  delay(100);
  
  if (mpu.begin()) {
    Serial.println("✓ MPU6050 hoạt động với SDA=4, SCL=5");
    setupMPU();
    return;
  }
  
  Serial.println("✗ Thất bại với SDA=4, SCL=5");
  Wire.end();
  
  Serial.println("❌ KHÔNG TÌM THẤY MPU6050!");
  Serial.println("Kiểm tra:");
  Serial.println("- Dây kết nối VCC, GND, SDA, SCL");
  Serial.println("- Địa chỉ I2C (thường là 0x68)");
  Serial.println("- Pull-up resistor trên SDA/SCL");
  
  while(1) {
    delay(1000);
  }
}

void setupMPU() {
  Serial.println("Cấu hình MPU6050...");
  
  // Cài đặt ranges
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  Serial.println("Range gia tốc: ±8G");
  Serial.println("Range con quay: ±500°/s");
  Serial.println("Filter bandwidth: 21Hz");
  Serial.println("✅ MPU6050 sẵn sàng!");
  Serial.println("Bắt đầu đọc dữ liệu...\n");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  Serial.print("Acc X:");
  Serial.print(a.acceleration.x, 2);
  Serial.print(" Y:");
  Serial.print(a.acceleration.y, 2);
  Serial.print(" Z:");
  Serial.print(a.acceleration.z, 2);
  Serial.print(" m/s²");
  
  Serial.print(" | Gyro X:");
  Serial.print(g.gyro.x, 2);
  Serial.print(" Y:");
  Serial.print(g.gyro.y, 2);
  Serial.print(" Z:");
  Serial.print(g.gyro.z, 2);
  Serial.print(" rad/s");
  
  Serial.print(" | Temp:");
  Serial.print(temp.temperature, 1);
  Serial.println("°C");
  
  delay(500);
} 