/*
 * dc_motor - OPTIONAL module (FEATURE_DC_MOTOR, off by default).
 *
 * The circuit diagram (slide 15) shows a DC motor switched by a MOSFET,
 * but the slides do not define its role. Here it is a generic PWM output,
 * driven by the alerts module as a stronger haptic during crash/drowsiness
 * alerts. A logic-level MOSFET and a flyback diode across the motor are required.
 */
#pragma once

#include <Arduino.h>

void dcMotorInit();
void dcMotorSet(float duty);  // 0.0 - 1.0
