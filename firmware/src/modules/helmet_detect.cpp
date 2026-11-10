#include "helmet_detect.h"

#include "config.h"

static bool s_worn = false;
static uint32_t s_candidateSinceMs = 0;  // when the raw reading started disagreeing with s_worn
static bool s_candidateActive = false;

void helmetDetectInit() {
  pinMode(PIN_HELMET_IR, INPUT);
  analogSetPinAttenuation(PIN_HELMET_IR, ADC_11db);  // full 0-3.3 V range
  s_worn = false;  // fail-safe: assume no helmet until proven otherwise
}

static uint16_t readAveraged() {
  uint32_t sum = 0;
  for (int i = 0; i < HELMET_ADC_SAMPLES; ++i) sum += analogRead(PIN_HELMET_IR);
  return static_cast<uint16_t>(sum / HELMET_ADC_SAMPLES);
}

HelmetReading helmetDetectUpdate(uint32_t nowMs) {
  const uint16_t raw = readAveraged();

  // Hysteresis: the level needed to change state depends on the current state.
  const bool wantsChange = s_worn ? (raw < HELMET_OFF_THRESHOLD) : (raw > HELMET_ON_THRESHOLD);

  if (!wantsChange) {
    s_candidateActive = false;
  } else {
    if (!s_candidateActive) {
      s_candidateActive = true;
      s_candidateSinceMs = nowMs;
    }
    const uint32_t needed = s_worn ? HELMET_OFF_DEBOUNCE_MS : HELMET_ON_DEBOUNCE_MS;
    if (nowMs - s_candidateSinceMs >= needed) {
      s_worn = !s_worn;
      s_candidateActive = false;
    }
  }
  return HelmetReading{raw, s_worn};
}
