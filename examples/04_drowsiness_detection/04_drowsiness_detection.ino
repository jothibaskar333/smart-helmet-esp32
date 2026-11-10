/*
 * 04_drowsiness_detection.ino
 * ------------------------------------------------------------------
 * Active-Safety Helmet - initial code sketch 4 (presentation slide 25)
 * Fatigue / drowsiness detection from head inactivity (MPU-6050).
 *
 * Board  : ESP32 Dev Module (esp32dev)
 * Library: "MPU6050" by Electronic Cats (i2cdevlib port), provides <MPU6050.h>
 * Wiring : MPU-6050 SDA -> GPIO 21, SCL -> GPIO 22, VCC 3.3 V, GND
 *          Buzzer -> GPIO 27
 *
 * Behaviour: a magnitude above 20000 LSB (about 1.22 g at +/-2 g) counts as
 *            "movement" and refreshes lastMove. If no movement is seen for
 *            more than 6000 ms, the buzzer sounds until the head moves again.
 * Serial   : the magnitude every 120 ms.
 *
 * Reproduced exactly from the slide; only these comments were added.
 */

//Fatigue / Drowsiness Detection Using IMU
#include <Wire.h>
#include <MPU6050.h>

MPU6050 imu;

#define BUZZER 27

unsigned long lastMove = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  imu.initialize();
  pinMode(BUZZER, OUTPUT);
}

void loop() {
  int16_t ax, ay, az;
  imu.getAcceleration(&ax, &ay, &az);

  float mag = sqrt(ax*ax + ay*ay + az*az);

  if (mag > 20000) lastMove = millis();

  digitalWrite(BUZZER, millis() - lastMove > 6000);

  Serial.println(mag);
  delay(120);
}
