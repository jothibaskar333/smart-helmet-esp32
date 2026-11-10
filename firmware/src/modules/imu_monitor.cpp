#include "imu_monitor.h"

#include <MPU6050.h>
#include <Wire.h>
#include <math.h>

#include "config.h"
#include "system_state.h"

static MPU6050 s_imu;
static bool s_ok = false;

// ---- filter state (owned by the IMU task) ----
static float s_pitch = 0.0f;
static float s_base = 0.0f;
static bool s_filterInit = false;
static uint32_t s_lastStepMs = 0;
static uint32_t s_initMs = 0;
static volatile uint32_t s_lastMoveMs = 0;  // written by IMU task, read by drowsiness task

// ---- crash state (owned by the IMU task) ----
enum CrashPhase : uint8_t { CP_IDLE, CP_SETTLE, CP_STILL_CHECK, CP_LATCHED };
static CrashPhase s_phase = CP_IDLE;
static uint8_t s_overCount = 0;
static uint32_t s_phaseStartMs = 0;
static uint32_t s_stillSamples = 0;
static uint32_t s_totalSamples = 0;
static volatile bool s_crashLatched = false;
static volatile bool s_clearRequested = false;

static constexpr float RAD2DEG = 57.2957795f;

bool imuInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  s_imu.initialize();
  s_ok = s_imu.testConnection();
  if (s_ok) {
    s_imu.setFullScaleAccelRange(MPU6050_ACCEL_FS_16);  // 2048 LSB/g
    s_imu.setFullScaleGyroRange(MPU6050_GYRO_FS_500);   // 65.5 LSB/(deg/s)
    s_imu.setDLPFMode(MPU6050_DLPF_BW_42);              // tame vibration noise
  }
  s_initMs = millis();
  s_lastMoveMs = s_initMs;
  return s_ok;
}

bool imuCrashLatched() { return s_crashLatched; }
void imuRequestCrashClear() { s_clearRequested = true; }

static void latchCrash() {
  s_phase = CP_LATCHED;
  s_crashLatched = true;
  alertSet(ALERT_IMPACT, false, EVT_IMPACT);
  alertSet(ALERT_CRASH, true, EVT_CRASH);
}

static void crashStep(uint32_t nowMs, float accelG, float gyroDps, bool armed) {
  if (s_clearRequested) {
    s_clearRequested = false;
    s_phase = CP_IDLE;
    s_overCount = 0;
    s_crashLatched = false;
    alertSet(ALERT_IMPACT, false, EVT_IMPACT);
    alertSet(ALERT_CRASH, false, EVT_CRASH);
    return;
  }

  switch (s_phase) {
    case CP_IDLE:
      if (!armed) {
        s_overCount = 0;
        break;
      }
      if (accelG > CRASH_SEVERE_G) {
        latchCrash();
      } else if (accelG > CRASH_IMPACT_G) {
        if (++s_overCount >= CRASH_IMPACT_MIN_SAMPLES) {
          s_phase = CP_SETTLE;
          s_phaseStartMs = nowMs;
          alertSet(ALERT_IMPACT, true, EVT_IMPACT);
        }
      } else {
        s_overCount = 0;
      }
      break;

    case CP_SETTLE:
      if (accelG > CRASH_SEVERE_G) {
        latchCrash();
      } else if (nowMs - s_phaseStartMs >= CRASH_SETTLE_MS) {
        s_phase = CP_STILL_CHECK;
        s_phaseStartMs = nowMs;
        s_stillSamples = 0;
        s_totalSamples = 0;
      }
      break;

    case CP_STILL_CHECK: {
      const bool still = fabsf(accelG - 1.0f) < CRASH_STILL_ACC_TOL_G && gyroDps < CRASH_STILL_GYRO_DPS;
      ++s_totalSamples;
      if (still) ++s_stillSamples;
      if (nowMs - s_phaseStartMs >= CRASH_STILL_WINDOW_MS) {
        if (s_stillSamples >= CRASH_STILL_MIN_RATIO * s_totalSamples) {
          latchCrash();
        } else {
          // Rider kept moving normally: treat it as a bump, not a crash.
          s_phase = CP_IDLE;
          s_overCount = 0;
          alertSet(ALERT_IMPACT, false, EVT_IMPACT);
        }
      }
      break;
    }

    case CP_LATCHED:
      break;  // only a clear request leaves this state
  }
}

ImuReading imuStep(uint32_t nowMs, bool armed) {
  if (!s_ok) return ImuReading{0, 0, s_pitch, s_base};

  int16_t rax, ray, raz, rgx, rgy, rgz;
  s_imu.getMotion6(&rax, &ray, &raz, &rgx, &rgy, &rgz);

  // Convert to physical units in float: no int overflow (see slide 24 note).
  const float ax = rax / IMU_ACCEL_LSB_PER_G;
  const float ay = ray / IMU_ACCEL_LSB_PER_G;
  const float az = raz / IMU_ACCEL_LSB_PER_G;
  const float gy = rgy / IMU_GYRO_LSB_PER_DPS;
  const float gx = rgx / IMU_GYRO_LSB_PER_DPS;
  const float gz = rgz / IMU_GYRO_LSB_PER_DPS;

  const float accelG = sqrtf(ax * ax + ay * ay + az * az);
  const float gyroDps = sqrtf(gx * gx + gy * gy + gz * gz);

  // Pitch: X forward, Z up. Nose-down rotates about +Y, accel X goes negative.
  const float pitchAcc = IMU_PITCH_SIGN * atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD2DEG;
  const float dt = s_lastStepMs ? (nowMs - s_lastStepMs) / 1000.0f : IMU_PERIOD_MS / 1000.0f;
  s_lastStepMs = nowMs;

  if (!s_filterInit) {
    s_pitch = pitchAcc;
    s_base = pitchAcc;
    s_filterInit = true;
  } else {
    s_pitch = IMU_COMP_ALPHA * (s_pitch + IMU_PITCH_SIGN * gy * dt) + (1.0f - IMU_COMP_ALPHA) * pitchAcc;
    // Baseline follows slow posture changes, but not nods (frozen while deviating).
    const bool settling = (nowMs - s_initMs) < 3000;  // converge quickly after boot
    const float tau = settling ? 0.5f : PITCH_BASELINE_TAU_S;
    if (settling || fabsf(s_pitch - s_base) < NOD_RELEASE_DEG) {
      s_base += (s_pitch - s_base) * (dt / tau);
    }
  }

  if (accelG > INACTIVITY_MOVE_G || gyroDps > INACTIVITY_MOVE_DPS) s_lastMoveMs = nowMs;

  crashStep(nowMs, accelG, gyroDps, armed);
  return ImuReading{accelG, gyroDps, s_pitch, s_base};
}

// ------------------------------------------------------------------
// Drowsiness (separate, lower-priority task)
// ------------------------------------------------------------------
static constexpr int MAX_NODS = 8;
static uint32_t s_nodTimes[MAX_NODS];
static uint8_t s_nodCount = 0;
static bool s_inNod = false;
static bool s_nodCounted = false;
static uint32_t s_nodStartMs = 0;
static uint32_t s_alertUntilMs = 0;

void drowsinessReset() {
  s_nodCount = 0;
  s_inNod = false;
  s_nodCounted = false;
  s_alertUntilMs = 0;
  s_lastMoveMs = millis();
  alertSet(ALERT_DROWSY, false, EVT_DROWSY);
}

static void recordNod(uint32_t nowMs) {
  if (s_nodCount == MAX_NODS) {  // drop the oldest
    memmove(s_nodTimes, s_nodTimes + 1, sizeof(uint32_t) * (MAX_NODS - 1));
    --s_nodCount;
  }
  s_nodTimes[s_nodCount++] = nowMs;
}

static void pruneNods(uint32_t nowMs) {
  uint8_t keep = 0;
  for (uint8_t i = 0; i < s_nodCount; ++i) {
    if (nowMs - s_nodTimes[i] <= NOD_WINDOW_MS) s_nodTimes[keep++] = s_nodTimes[i];
  }
  s_nodCount = keep;
}

uint8_t drowsinessStep(uint32_t nowMs, bool helmetWorn, float pitchDeg, float pitchBaseDeg) {
  if (!helmetWorn || !s_ok) {
    if (s_nodCount || s_inNod || alertActive(ALERT_DROWSY)) drowsinessReset();
    s_lastMoveMs = nowMs;  // don't count time off-head as inactivity
    return 0;
  }

  bool trigger = false;
  const float dev = pitchDeg - pitchBaseDeg;

  if (!s_inNod && dev > NOD_ANGLE_DEG) {
    s_inNod = true;
    s_nodCounted = false;
    s_nodStartMs = nowMs;
  }
  if (s_inNod) {
    const uint32_t held = nowMs - s_nodStartMs;
    if (!s_nodCounted && held >= NOD_MIN_MS) {
      s_nodCounted = true;
      recordNod(nowMs);
    }
    if (held >= HEAD_DROP_ALERT_MS) trigger = true;  // head hanging forward
    if (dev < NOD_RELEASE_DEG) s_inNod = false;
  }

  pruneNods(nowMs);
  if (s_nodCount >= NOD_COUNT_ALERT) {
    trigger = true;
    s_nodCount = 0;  // start a fresh window after alerting
  }

  // Slide 25 rule: no significant movement for 6 s.
  if (nowMs - s_lastMoveMs > INACTIVITY_ALERT_MS) trigger = true;

  if (trigger) s_alertUntilMs = nowMs + DROWSY_ALERT_HOLD_MS;
  alertSet(ALERT_DROWSY, (int32_t)(s_alertUntilMs - nowMs) > 0, EVT_DROWSY);
  return s_nodCount;
}
