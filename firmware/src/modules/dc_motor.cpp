#include "dc_motor.h"

#include "config.h"

#if FEATURE_DC_MOTOR
#include <ESP32PWM.h>  // from ESP32Servo: shares LEDC allocation with the fit servo

static ESP32PWM s_pwm;
static float s_duty = -1.0f;

void dcMotorInit() {
  s_pwm.attachPin(PIN_DC_MOTOR, 20000, 10);  // 20 kHz (inaudible), 10-bit
  dcMotorSet(0.0f);
}

void dcMotorSet(float duty) {
  duty = constrain(duty, 0.0f, 1.0f);
  if (duty == s_duty) return;
  s_duty = duty;
  s_pwm.writeScaled(duty);
}

#else
void dcMotorInit() {}
void dcMotorSet(float) {}
#endif
