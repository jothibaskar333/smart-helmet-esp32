#include "alerts.h"

#include "config.h"
#include "system_state.h"
#if FEATURE_DC_MOTOR
#include "dc_motor.h"
#endif

void alertsInit() {
  const uint8_t outs[] = {PIN_VIB_L, PIN_VIB_R, PIN_BUZZER, PIN_LED_WARN, PIN_LED_STATUS};
  for (uint8_t pin : outs) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
}

// true for `onMs` out of every `periodMs`
static inline bool blink(uint32_t nowMs, uint32_t periodMs, uint32_t onMs) {
  return (nowMs % periodMs) < onMs;
}

void alertsStep(uint32_t nowMs, uint32_t bits, bool bleConnected) {
  bool vibL = false, vibR = false, buzzer = false, led = false;
  float motorDuty = 0.0f;

  if (bits & ALERT_CRASH) {
    vibL = vibR = true;
    buzzer = blink(nowMs, 250, 125);
    led = blink(nowMs, 250, 125);
    motorDuty = DC_MOTOR_ALERT_DUTY;
  } else {
    if (bits & ALERT_DROWSY) {
      const bool pulse = blink(nowMs, 1000, 500);
      vibL = vibR = pulse;
      buzzer = blink(nowMs, 1000, 100);
      motorDuty = pulse ? DC_MOTOR_ALERT_DUTY : 0.0f;
    } else if (bits & ALERT_IMPACT) {
      buzzer = blink(nowMs, 1000, 60);
    }
    // Blind-spot haptics stay directional even during a drowsiness alert.
    if (bits & ALERT_BLIND_L) vibL = true;
    if (bits & ALERT_BLIND_R) vibR = true;
    if (bits & (ALERT_BLIND_L | ALERT_BLIND_R)) led = true;
    if ((bits & ALERT_IGN_GRACE) && !buzzer) buzzer = blink(nowMs, 1000, 40);
  }

  digitalWrite(PIN_VIB_L, vibL);
  digitalWrite(PIN_VIB_R, vibR);
  digitalWrite(PIN_BUZZER, buzzer);
  digitalWrite(PIN_LED_WARN, led);
  digitalWrite(PIN_LED_STATUS, bleConnected ? HIGH : blink(nowMs, 2000, 100));
#if FEATURE_DC_MOTOR
  dcMotorSet(motorDuty);
#else
  (void)motorDuty;
#endif
}
