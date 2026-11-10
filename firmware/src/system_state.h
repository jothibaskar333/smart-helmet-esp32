/*
 * system_state.h - state shared between FreeRTOS tasks, the alert event
 * group and the alert event queue.
 *
 * Writers update the fields they own under a mutex; readers take a full
 * snapshot copy. Continuous alert conditions are bits in an event group
 * (consumed by the alerts task), and edge events go into a queue
 * (consumed by the BLE task, which also logs them to serial).
 */
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

enum IgnitionState : uint8_t {
  IGN_LOCKED = 0,   // relay off
  IGN_ENABLED = 1,  // relay on
  IGN_GRACE = 2,    // helmet lost, relay still on until the grace period ends
};

// Continuous alert conditions (event-group bits)
enum AlertBits : EventBits_t {
  ALERT_CRASH         = 1 << 0,
  ALERT_DROWSY        = 1 << 1,
  ALERT_BLIND_L       = 1 << 2,
  ALERT_BLIND_R       = 1 << 3,
  ALERT_IGN_GRACE     = 1 << 4,
  ALERT_FIT_FAULT     = 1 << 5,
  ALERT_IMPACT        = 1 << 6,  // impact seen, crash confirmation in progress
  ALERT_ALL           = 0x7F,
};

// Discrete events, sent on every change of a condition
enum EventCode : uint8_t {
  EVT_HELMET = 0,
  EVT_IGNITION,
  EVT_CRASH,
  EVT_IMPACT,
  EVT_DROWSY,
  EVT_BLIND_L,
  EVT_BLIND_R,
  EVT_FIT_FAULT,
  EVT_BLE_CMD,
};

struct AppEvent {
  EventCode code;
  int32_t value;     // 1/0 for on/off conditions, state value otherwise
  uint32_t timeMs;
};

struct SystemState {
  // helmet + ignition
  uint16_t irRaw;
  bool helmetOn;
  IgnitionState ignition;
  // IMU
  float accelG;        // latest |a| in g
  float accelPeakG;    // peak |a| since the console last read it
  float gyroDps;       // latest |w| in deg/s
  float pitchDeg;      // filtered pitch (forward nod positive)
  float pitchBaseDeg;  // slow-moving baseline posture
  uint8_t nodCount;    // nods in the current window
  bool imuOk;
  // blind spot
  float distLcm;
  float distRcm;
  // fit
  uint16_t fsrRaw;
  uint8_t servoDeg;
  // optional
  uint8_t heartBpm;
  // comms
  bool bleConnected;
};

void stateInit();
SystemState stateSnapshot();
// Run fn with the state mutex held. Keep it short.
void stateUpdate(void (*fn)(SystemState&, void*), void* ctx);

// Convenience setters for single fields used by several tasks
void stateSetBleConnected(bool connected);
float stateTakeAccelPeak();

EventGroupHandle_t alertGroup();
// Set or clear an alert bit and post an event if the bit actually changed.
void alertSet(AlertBits bit, bool on, EventCode code);
bool alertActive(AlertBits bit);

void eventPost(EventCode code, int32_t value);
QueueHandle_t eventQueue();
const char* eventName(EventCode code);
