// === File: mpu6050_module.h ===
#ifndef MPU6050_MODULE_H
#define MPU6050_MODULE_H

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

class MPU6050Module {
  public:
    Adafruit_MPU6050 mpu;
    float accX, accY, accZ, gyroX, gyroY, gyroZ;

    bool begin(uint8_t sda = 21, uint8_t scl = 22) {
      Wire.begin(sda, scl);
      return mpu.begin();
    }

    void update() {
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      accX = a.acceleration.x;
      accY = a.acceleration.y;
      accZ = a.acceleration.z;
      gyroX = g.gyro.x;
      gyroY = g.gyro.y;
      gyroZ = g.gyro.z;
    }

    String serialize() {
      return String(accX, 2) + "," + String(accY, 2) + "," + String(accZ, 2) + "," +
             String(gyroX, 2) + "," + String(gyroY, 2) + "," + String(gyroZ, 2);
    }
};

#endif
