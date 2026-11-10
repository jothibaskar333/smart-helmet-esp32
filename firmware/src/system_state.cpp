#include "system_state.h"

#include <freertos/semphr.h>

#include "config.h"

static SystemState s_state;
static SemaphoreHandle_t s_mutex = nullptr;
static EventGroupHandle_t s_alerts = nullptr;
static QueueHandle_t s_events = nullptr;

void stateInit() {
  memset(&s_state, 0, sizeof(s_state));
  s_state.ignition = IGN_LOCKED;
  s_state.distLcm = BS_NO_ECHO_CM;
  s_state.distRcm = BS_NO_ECHO_CM;
  s_mutex = xSemaphoreCreateMutex();
  s_alerts = xEventGroupCreate();
  s_events = xQueueCreate(EVENT_QUEUE_LEN, sizeof(AppEvent));
  configASSERT(s_mutex && s_alerts && s_events);
}

SystemState stateSnapshot() {
  SystemState copy;
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  copy = s_state;
  xSemaphoreGive(s_mutex);
  return copy;
}

void stateUpdate(void (*fn)(SystemState&, void*), void* ctx) {
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  fn(s_state, ctx);
  xSemaphoreGive(s_mutex);
}

void stateSetBleConnected(bool connected) {
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  s_state.bleConnected = connected;
  xSemaphoreGive(s_mutex);
}

float stateTakeAccelPeak() {
  xSemaphoreTake(s_mutex, portMAX_DELAY);
  float peak = s_state.accelPeakG;
  s_state.accelPeakG = 0.0f;
  xSemaphoreGive(s_mutex);
  return peak;
}

EventGroupHandle_t alertGroup() { return s_alerts; }

void alertSet(AlertBits bit, bool on, EventCode code) {
  const bool was = (xEventGroupGetBits(s_alerts) & bit) != 0;
  if (was == on) return;
  if (on) {
    xEventGroupSetBits(s_alerts, bit);
  } else {
    xEventGroupClearBits(s_alerts, bit);
  }
  eventPost(code, on ? 1 : 0);
}

bool alertActive(AlertBits bit) { return (xEventGroupGetBits(s_alerts) & bit) != 0; }

void eventPost(EventCode code, int32_t value) {
  AppEvent ev{code, value, millis()};
  // Never block a sensor task: if the queue is full the oldest consumer is
  // behind, and the continuous state is still visible in the event group.
  xQueueSend(s_events, &ev, 0);
}

QueueHandle_t eventQueue() { return s_events; }

const char* eventName(EventCode code) {
  switch (code) {
    case EVT_HELMET:    return "HELMET";
    case EVT_IGNITION:  return "IGNITION";
    case EVT_CRASH:     return "CRASH";
    case EVT_IMPACT:    return "IMPACT";
    case EVT_DROWSY:    return "DROWSY";
    case EVT_BLIND_L:   return "BLIND_L";
    case EVT_BLIND_R:   return "BLIND_R";
    case EVT_FIT_FAULT: return "FIT_FAULT";
    case EVT_BLE_CMD:   return "BLE_CMD";
  }
  return "?";
}
