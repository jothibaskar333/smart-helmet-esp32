/*
 * 01_blind_spot_single.ino
 * ------------------------------------------------------------------
 * Active-Safety Helmet - initial code sketch 1 (presentation slide 22)
 * Blind-spot detection with one HC-SR04 ultrasonic sensor + haptic alert.
 *
 * Board : ESP32 Dev Module (esp32dev)
 * Wiring: HC-SR04 TRIG -> GPIO 5
 *         HC-SR04 ECHO -> GPIO 18 (through a 1k/2k voltage divider: ECHO is 5 V)
 *         Vibration motor driver input -> GPIO 25 (via transistor, not direct)
 *
 * Behaviour: the motor vibrates while an object is 10-80 cm away.
 *            A pulseIn() timeout returns 0 cm, which the "> 10" check ignores.
 * Serial   : one distance (cm) per line at 115200 baud, every 150 ms.
 *
 * Reproduced exactly from the slide; only these comments were added.
 */

//Blind-Spot Detection Using Ultrasonic + Haptic Alert
#define TRIG_PIN 5
#define ECHO_PIN 18
#define VIB_PIN 25

long duration;

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration * 0.034 / 2.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(VIB_PIN, OUTPUT);
}

void loop() {
  float d = readDistance();

  digitalWrite(VIB_PIN, d < 80 && d > 10);

  Serial.println(d);
  delay(150);
}
