/*
 * Active-Safety Helmet - integrated firmware (ESP32 + FreeRTOS)
 *
 * Modules (src/modules) hold the logic. This file owns scheduling: one
 * FreeRTOS task per safety function, each paced with vTaskDelayUntil.
 *
 *   task            prio  period   job
 *   imu_crash        6    10 ms    MPU-6050 sampling, pitch filter, crash state machine
 *   alerts           5    20 ms    event-group bits -> vibration / buzzer / LED patterns
 *   helmet_ign       5    200 ms   IR helmet detection + fail-safe ignition relay
 *   blind_spot       4    150 ms   left/right ultrasonic, median filter
 *   drowsiness       3    120 ms   nod / head-drop / inactivity detection
 *   fit              2    250 ms   FSR -> servo strap tension
 *   heart_rate       2    20 ms    optional pulse sensor
 *   ble              1    500 ms   status notify + event-queue dispatch (also logs events)
 *   console          1    100 ms   CSV stream, serial commands, BOOT long-press
 *
 * Alerts flow: modules set/clear bits in an event group (continuous state
 * for the alerts task) and post AppEvents to a queue on every change
 * (one-shot notifications for BLE + serial log).
 */
#include <Arduino.h>

#include "config.h"
#include "modules/alerts.h"
#include "modules/ble_service.h"
#include "modules/blind_spot.h"
#include "modules/dc_motor.h"
#include "modules/fit_control.h"
#include "modules/heart_rate.h"
#include "modules/helmet_detect.h"
#include "modules/ignition_relay.h"
#include "modules/imu_monitor.h"
#include "system_state.h"

static constexpr uint32_t STACK_WORDS = 4096;
static volatile bool s_csvEnabled = DEBUG_CSV;

static inline uint32_t nowMs() { return millis(); }

// ---------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------
static void imuTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    const SystemState snap = stateSnapshot();
    const bool armed = snap.helmetOn || snap.ignition != IGN_LOCKED;
    ImuReading r = imuStep(nowMs(), armed);
    stateUpdate(
        [](SystemState& s, void* ctx) {
          const ImuReading& r = *static_cast<ImuReading*>(ctx);
          s.accelG = r.accelG;
          if (r.accelG > s.accelPeakG) s.accelPeakG = r.accelG;
          s.gyroDps = r.gyroDps;
          s.pitchDeg = r.pitchDeg;
          s.pitchBaseDeg = r.pitchBaseDeg;
        },
        &r);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(IMU_PERIOD_MS));
  }
}

static void helmetIgnitionTask(void*) {
  TickType_t last = xTaskGetTickCount();
  bool prevWorn = false;
  for (;;) {
    const uint32_t t = nowMs();
    HelmetReading h = helmetDetectUpdate(t);
    if (h.worn != prevWorn) {
      prevWorn = h.worn;
      eventPost(EVT_HELMET, h.worn);
    }
    const IgnitionState ign = ignitionUpdate(h.worn, imuCrashLatched(), t);
    struct Ctx { HelmetReading h; IgnitionState ign; } ctx{h, ign};
    stateUpdate(
        [](SystemState& s, void* p) {
          const Ctx& c = *static_cast<Ctx*>(p);
          s.irRaw = c.h.raw;
          s.helmetOn = c.h.worn;
          s.ignition = c.ign;
        },
        &ctx);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(HELMET_PERIOD_MS));
  }
}

static void blindSpotTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    BlindSpotReading r = blindSpotStep();
    const bool worn = stateSnapshot().helmetOn;  // don't buzz a helmet sitting on a shelf
    alertSet(ALERT_BLIND_L, worn && r.alertLeft, EVT_BLIND_L);
    alertSet(ALERT_BLIND_R, worn && r.alertRight, EVT_BLIND_R);
    stateUpdate(
        [](SystemState& s, void* p) {
          const BlindSpotReading& r = *static_cast<BlindSpotReading*>(p);
          s.distLcm = r.leftCm;
          s.distRcm = r.rightCm;
        },
        &r);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(BS_PERIOD_MS));
  }
}

static void drowsinessTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    const SystemState snap = stateSnapshot();
    uint8_t nods = drowsinessStep(nowMs(), snap.helmetOn, snap.pitchDeg, snap.pitchBaseDeg);
    stateUpdate([](SystemState& s, void* p) { s.nodCount = *static_cast<uint8_t*>(p); }, &nods);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(DROWSY_PERIOD_MS));
  }
}

#if FEATURE_FIT_CONTROL
static void fitTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    FitReading r = fitStep(stateSnapshot().helmetOn, imuCrashLatched());
    stateUpdate(
        [](SystemState& s, void* p) {
          const FitReading& r = *static_cast<FitReading*>(p);
          s.fsrRaw = r.fsrRaw;
          s.servoDeg = r.servoDeg;
        },
        &r);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(FIT_PERIOD_MS));
  }
}
#endif

#if FEATURE_HEART_RATE
static void heartRateTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    uint8_t bpm = heartRateStep(nowMs());
    stateUpdate([](SystemState& s, void* p) { s.heartBpm = *static_cast<uint8_t*>(p); }, &bpm);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(HR_PERIOD_MS));
  }
}
#endif

static void alertsTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    alertsStep(nowMs(), xEventGroupGetBits(alertGroup()), stateSnapshot().bleConnected);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(ALERT_TICK_MS));
  }
}

// Waits on the event queue; sends status on a fixed schedule in between.
static void bleTask(void*) {
  TickType_t nextStatus = xTaskGetTickCount();
  for (;;) {
    const TickType_t now = xTaskGetTickCount();
    const TickType_t wait = (nextStatus > now) ? nextStatus - now : 0;
    AppEvent ev;
    if (xQueueReceive(eventQueue(), &ev, wait) == pdTRUE) {
      char line[48];
      snprintf(line, sizeof(line), "# EVT %lu %s %ld\n", static_cast<unsigned long>(ev.timeMs),
               eventName(ev.code), static_cast<long>(ev.value));
      Serial.print(line);
      bleSendEvent(ev);
      continue;
    }
    bleSendStatus(stateSnapshot(), xEventGroupGetBits(alertGroup()));
    nextStatus = xTaskGetTickCount() + pdMS_TO_TICKS(BLE_STATUS_PERIOD_MS);
  }
}

static void printCsvHeader() {
  Serial.print("H,t_ms,ir,helmet,ign,dl_cm,dr_cm,acc_g,acc_peak_g,gyro_dps,pitch,pitch_base,nods,fsr,servo,hr,alerts\n");
}

static void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd == "clear") {
    imuRequestCrashClear();
    Serial.print("# crash latch cleared (serial)\n");
  } else if (cmd == "csv on") {
    s_csvEnabled = true;
    printCsvHeader();
  } else if (cmd == "csv off") {
    s_csvEnabled = false;
  } else if (cmd == "status") {
    const SystemState s = stateSnapshot();
    char line[160];
    snprintf(line, sizeof(line),
             "# helmet=%d ign=%s crash=%d imu=%d L=%.0fcm R=%.0fcm pitch=%.1f base=%.1f fsr=%u servo=%u ble=%d\n",
             s.helmetOn, ignitionName(s.ignition), imuCrashLatched(), s.imuOk, s.distLcm, s.distRcm,
             s.pitchDeg, s.pitchBaseDeg, s.fsrRaw, s.servoDeg, s.bleConnected);
    Serial.print(line);
  } else if (cmd.length()) {
    Serial.print("# commands: clear | status | csv on | csv off\n");
  }
}

static void consoleTask(void*) {
  TickType_t last = xTaskGetTickCount();
  String input;
  uint32_t pressStart = 0;
  uint32_t lastHeader = 0;
  for (;;) {
    const uint32_t t = nowMs();

    // Serial commands (line based)
    while (Serial.available()) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\n' || c == '\r') {
        handleCommand(input);
        input = "";
      } else if (input.length() < 32) {
        input += c;
      }
    }

    // BOOT button long-press clears a latched crash
    if (digitalRead(PIN_CLEAR_BUTTON) == LOW) {
      if (!pressStart) pressStart = t;
      if (pressStart != UINT32_MAX && t - pressStart >= CLEAR_LONG_PRESS_MS) {
        imuRequestCrashClear();
        Serial.print("# crash latch cleared (button)\n");
        pressStart = UINT32_MAX;  // one clear per press
      }
    } else {
      pressStart = 0;
    }

    if (s_csvEnabled) {
      if (t - lastHeader > 10000 || !lastHeader) {  // re-send so late-attaching tools get it
        printCsvHeader();
        lastHeader = t;
      }
      const SystemState s = stateSnapshot();
      const float peak = stateTakeAccelPeak();
      char line[200];
      snprintf(line, sizeof(line), "D,%lu,%u,%d,%d,%.1f,%.1f,%.3f,%.3f,%.1f,%.2f,%.2f,%u,%u,%u,%u,%lu\n",
               static_cast<unsigned long>(t), s.irRaw, s.helmetOn, s.ignition, s.distLcm, s.distRcm,
               s.accelG, peak, s.gyroDps, s.pitchDeg, s.pitchBaseDeg, s.nodCount, s.fsrRaw, s.servoDeg,
               s.heartBpm, static_cast<unsigned long>(xEventGroupGetBits(alertGroup())));
      Serial.print(line);
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(CONSOLE_PERIOD_MS));
  }
}

static void startTask(TaskFunction_t fn, const char* name, UBaseType_t prio) {
  const BaseType_t ok = xTaskCreatePinnedToCore(fn, name, STACK_WORDS, nullptr, prio, nullptr, APP_CORE);
  configASSERT(ok == pdPASS);
}

// ---------------------------------------------------------------------
void setup() {
  // Fail-safe first: ignition relay OFF before anything else can fail.
  ignitionInit();

  Serial.begin(115200);
  stateInit();
  alertsInit();
  pinMode(PIN_CLEAR_BUTTON, INPUT_PULLUP);
  helmetDetectInit();
  blindSpotInit();

  bool imuOk = imuInit();
  stateUpdate([](SystemState& s, void* p) { s.imuOk = *static_cast<bool*>(p); }, &imuOk);
  Serial.printf("# Active-Safety Helmet firmware | IMU %s\n", imuOk ? "OK" : "NOT FOUND (crash/drowsy disabled)");

#if FEATURE_FIT_CONTROL
  fitInit();
#endif
  heartRateInit();
  dcMotorInit();
  bleInit();

  startTask(imuTask, "imu_crash", PRIO_IMU_CRASH);
  startTask(alertsTask, "alerts", PRIO_ALERTS);
  startTask(helmetIgnitionTask, "helmet_ign", PRIO_HELMET_IGNITION);
  startTask(blindSpotTask, "blind_spot", PRIO_BLIND_SPOT);
  startTask(drowsinessTask, "drowsiness", PRIO_DROWSINESS);
#if FEATURE_FIT_CONTROL
  startTask(fitTask, "fit", PRIO_FIT);
#endif
#if FEATURE_HEART_RATE
  startTask(heartRateTask, "heart_rate", PRIO_HEART_RATE);
#endif
  startTask(bleTask, "ble", PRIO_BLE);
  startTask(consoleTask, "console", PRIO_CONSOLE);
}

void loop() {
  vTaskDelete(nullptr);  // all work happens in the tasks above
}
