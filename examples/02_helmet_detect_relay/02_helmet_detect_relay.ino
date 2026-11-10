/*
 * 02_helmet_detect_relay.ino
 * ------------------------------------------------------------------
 * Active-Safety Helmet - initial code sketch 2 (presentation slide 23)
 * Helmet-wear detection with an analog IR proximity sensor + ignition relay.
 *
 * Board : ESP32 Dev Module (esp32dev)
 * Wiring: Sharp IR proximity sensor analog out -> GPIO 34 (ADC1, input-only)
 *         Relay module IN -> GPIO 14
 *
 * Behaviour: analogRead() > 1500 (of 0-4095) means a head is inside the
 *            helmet -> relay ON (ignition allowed); otherwise relay OFF.
 * Serial   : "1" (helmet on) or "0" (helmet off) every 200 ms.
 *
 * Note: this sketch has no debounce. The integrated firmware in /firmware
 * adds hysteresis and a grace period (see docs/hardware.md).
 *
 * Reproduced exactly from the slide; only these comments were added.
 */

#define HELMET_SENSOR 34
#define RELAY_PIN 14

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  bool helmetOn = analogRead(HELMET_SENSOR) > 1500;
  digitalWrite(RELAY_PIN, helmetOn ? HIGH : LOW);

  Serial.println(helmetOn);
  delay(200);
}
