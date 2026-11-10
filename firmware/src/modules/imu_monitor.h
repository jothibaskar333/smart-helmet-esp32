/*
 * imu_monitor - MPU-6050 sampling, crash detection and drowsiness detection.
 *
 * imuStep() runs at 100 Hz in the highest-priority task:
 *   - converts raw counts to g and deg/s (ranges set explicitly: +/-16 g, +/-500 dps)
 *   - complementary filter -> pitch, plus a slow baseline of the rider's posture
 *   - crash state machine: IDLE -> impact (N samples > CRASH_IMPACT_G)
 *       -> settle -> stillness check -> LATCHED (or back to IDLE if the rider moves).
 *       An impact above CRASH_SEVERE_G latches immediately.
 *       The latch is cleared only by imuRequestCrashClear() (BOOT long-press,
 *       serial "clear" or BLE command "CLEAR").
 *
 * drowsinessStep() runs at the slide's 120 ms period:
 *   - nod detection: forward pitch beyond NOD_ANGLE_DEG for >= NOD_MIN_MS,
 *     NOD_COUNT_ALERT nods within NOD_WINDOW_MS -> alert
 *   - head drop: pitch held beyond NOD_ANGLE_DEG for HEAD_DROP_ALERT_MS -> alert
 *   - inactivity (slide 25 rule): no movement for INACTIVITY_ALERT_MS -> alert
 */
#pragma once

#include <Arduino.h>

struct ImuReading {
  float accelG;
  float gyroDps;
  float pitchDeg;
  float pitchBaseDeg;
};

bool imuInit();
// armed: crash detection only starts a new impact sequence while the helmet is
// worn or the ignition is on, so dropping the helmet on a table is ignored.
ImuReading imuStep(uint32_t nowMs, bool armed);
bool imuCrashLatched();
void imuRequestCrashClear();

void drowsinessReset();
// Returns the current number of nods in the window; sets/clears ALERT_DROWSY.
uint8_t drowsinessStep(uint32_t nowMs, bool helmetWorn, float pitchDeg, float pitchBaseDeg);
