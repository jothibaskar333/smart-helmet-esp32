/*
 * 03_crash_detection.ino
 * ------------------------------------------------------------------
 * Active-Safety Helmet - initial code sketch 3 (presentation slide 24)
 * Crash / impact detection with an MPU-6050 accelerometer.
 *
 * Board  : ESP32 Dev Module (esp32dev)
 * Library: "MPU6050" by Electronic Cats (i2cdevlib port), provides <MPU6050.h>
 * Wiring : MPU-6050 SDA -> GPIO 21, SCL -> GPIO 22, VCC 3.3 V, GND
 *          Buzzer -> GPIO 27, vibration motor driver -> GPIO 25
 *
 * Behaviour: the raw acceleration magnitude sqrt(ax^2+ay^2+az^2) is in LSB.
 *            At the default +/-2 g range 1 g = 16384 LSB, so the 35000
 *            threshold is about 2.1 g. Above it, buzzer + vibration turn on
 *            (only for that 80 ms sample; there is no latch).
 *            Note: the int16 squares are computed as 32-bit int, so the sum
 *            overflows once the magnitude exceeds about 46341 LSB (~2.8 g).
 *            sqrt() of the negative result is NaN, and a hard impact sampled
 *            above ~2.8 g can be missed. The integrated firmware uses floats in g.
 * Serial   : the magnitude every 80 ms (about 16384 when stationary).
 *
 * Reproduced exactly from the slide; only these comments were added.
 */

//Crash / Impact Detection Using IMU
#include <Wire.h>
#include <MPU6050.h>

MPU6050 imu;

#define BUZZER 27
#define VIB 25

void setup() {
  Serial.begin(115200);
  Wire.begin();
  imu.initialize();
  pinMode(BUZZER, OUTPUT);
  pinMode(VIB, OUTPUT);
}

void loop() {
  int16_t ax, ay, az;
  imu.getAcceleration(&ax, &ay, &az);

  float mag = sqrt(ax*ax + ay*ay + az*az);

  bool crash = mag > 35000;

  digitalWrite(BUZZER, crash);
  digitalWrite(VIB, crash);

  Serial.println(mag);
  delay(80);
}
