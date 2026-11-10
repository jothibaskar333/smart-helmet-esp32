/*
 * fit_control - auto-tension adaptive fit (slide 14).
 *
 * Hysteresis controller: the FSR (strap pressure) is kept inside
 * [FIT_TARGET_MIN, FIT_TARGET_MAX]. Below the band the servo tightens by
 * FIT_STEP_DEG per period, above it loosens, inside it holds. The angle is
 * clamped to [FIT_SERVO_LOOSE_DEG, FIT_SERVO_TIGHT_DEG], so the servo can
 * never over-tighten. An out-of-range FSR reading (open circuit or
 * saturated) holds the current position and raises ALERT_FIT_FAULT.
 *
 * Helmet removed -> release to the loose limit, ready for the next wear.
 * Crash latched (FIT_RELEASE_ON_CRASH) -> release, so the helmet is easier
 * to remove ("Emergency Release Mechanism" in the block diagram).
 */
#pragma once

#include <Arduino.h>

struct FitReading {
  uint16_t fsrRaw;
  uint8_t servoDeg;
  bool fault;
};

void fitInit();
FitReading fitStep(bool helmetWorn, bool crashLatched);
