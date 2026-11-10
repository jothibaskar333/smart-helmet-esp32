/*
 * 05_blind_spot_dual.ino
 * ------------------------------------------------------------------
 * Active-Safety Helmet - initial code sketch 5 (presentation slide 26)
 * Dual-side (left / right) blind-spot detection with per-side haptics.
 *
 * Board : ESP32 Dev Module (esp32dev)
 * Wiring: Left  HC-SR04 TRIG -> GPIO 5,  ECHO -> GPIO 18
 *         Right HC-SR04 TRIG -> GPIO 19, ECHO -> GPIO 21
 *         (both ECHO lines through a 1k/2k voltage divider: ECHO is 5 V)
 *         Left vibration motor driver -> GPIO 25, right -> GPIO 26
 *
 * Behaviour: each side vibrates while an object is 10-70 cm away.
 * Serial   : "left,right" distances in cm every 150 ms.
 *
 * NOTE: GPIO 21 is the ESP32's default I2C SDA line. This standalone sketch
 * does not use I2C, so it works as-is, but in the integrated firmware the
 * MPU-6050 needs GPIO 21. There, ECHO_R moves to GPIO 23 (docs/hardware.md).
 *
 * Reproduced exactly from the slide; only these comments were added.
 */

//Dual-Side Blind-Spot Detection (Left / Right)
#define TRIG_L 5
#define ECHO_L 18
#define TRIG_R 19
#define ECHO_R 21

#define VIB_L 25
#define VIB_R 26

float getDist(int t, int e) {
  digitalWrite(t, LOW);
  delayMicroseconds(2);
  digitalWrite(t, HIGH);
  delayMicroseconds(10);
  digitalWrite(t, LOW);
  long d = pulseIn(e, HIGH, 30000);
  return d * 0.034 / 2.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_L, OUTPUT);
  pinMode(ECHO_L, INPUT);
  pinMode(TRIG_R, OUTPUT);
  pinMode(ECHO_R, INPUT);
  pinMode(VIB_L, OUTPUT);
  pinMode(VIB_R, OUTPUT);
}

void loop() {
  float dl = getDist(TRIG_L, ECHO_L);
  float dr = getDist(TRIG_R, ECHO_R);

  digitalWrite(VIB_L, dl < 70 && dl > 10);
  digitalWrite(VIB_R, dr < 70 && dr > 10);

  Serial.print(dl);
  Serial.print(",");
  Serial.println(dr);

  delay(150);
}
