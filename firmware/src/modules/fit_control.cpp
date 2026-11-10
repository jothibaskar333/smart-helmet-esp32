#include "fit_control.h"

#include <ESP32Servo.h>

#include "config.h"
#include "system_state.h"

static_assert(FIT_SERVO_LOOSE_DEG < FIT_SERVO_TIGHT_DEG, "loose limit must be below tight limit");
static_assert(FIT_TARGET_MIN < FIT_TARGET_MAX, "empty FSR target band");

static Servo s_servo;
static int s_angle = FIT_SERVO_LOOSE_DEG;

static uint16_t readFsr() {
  uint32_t sum = 0;
  for (int i = 0; i < FIT_ADC_SAMPLES; ++i) sum += analogRead(PIN_FSR);
  return static_cast<uint16_t>(sum / FIT_ADC_SAMPLES);
}

static void moveTo(int angle) {
  angle = constrain(angle, FIT_SERVO_LOOSE_DEG, FIT_SERVO_TIGHT_DEG);  // hard limits
  if (angle != s_angle) {
    s_angle = angle;
    s_servo.write(s_angle);
  }
}

void fitInit() {
  pinMode(PIN_FSR, INPUT);
  analogSetPinAttenuation(PIN_FSR, ADC_11db);
  s_servo.setPeriodHertz(50);
  s_servo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
  s_angle = FIT_SERVO_LOOSE_DEG;
  s_servo.write(s_angle);  // start fully loose
}

FitReading fitStep(bool helmetWorn, bool crashLatched) {
  const uint16_t fsr = readFsr();
  const bool valid = fsr >= FIT_VALID_MIN && fsr <= FIT_VALID_MAX;
  bool fault = false;

  if ((FIT_RELEASE_ON_CRASH && crashLatched) || !helmetWorn) {
    moveTo(FIT_SERVO_LOOSE_DEG);
  } else if (!valid) {
    fault = true;  // hold position: never act on a reading we can't trust
  } else if (fsr < FIT_TARGET_MIN) {
    moveTo(s_angle + FIT_STEP_DEG);  // too loose -> tighten one step
  } else if (fsr > FIT_TARGET_MAX) {
    moveTo(s_angle - FIT_STEP_DEG);  // too tight -> loosen one step
  }
  alertSet(ALERT_FIT_FAULT, fault, EVT_FIT_FAULT);
  return FitReading{fsr, static_cast<uint8_t>(s_angle), fault};
}
