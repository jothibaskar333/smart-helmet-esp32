#include "heart_rate.h"

#include "config.h"

#if FEATURE_HEART_RATE

static float s_min = 4095, s_max = 0;
static bool s_above = false;
static uint32_t s_lastBeatMs = 0;
static uint32_t s_ibi[5] = {0};
static uint8_t s_ibiCount = 0, s_ibiNext = 0;
static uint8_t s_bpm = 0;

void heartRateInit() {
  pinMode(PIN_HEART_RATE, INPUT);
  analogSetPinAttenuation(PIN_HEART_RATE, ADC_11db);
}

uint8_t heartRateStep(uint32_t nowMs) {
  const float v = analogRead(PIN_HEART_RATE);

  // Envelope that slowly relaxes toward the signal
  s_min = (v < s_min) ? v : s_min + (v - s_min) * 0.01f;
  s_max = (v > s_max) ? v : s_max - (s_max - v) * 0.01f;
  const float amplitude = s_max - s_min;
  const float threshold = s_min + amplitude * 0.5f;

  if (amplitude > 100 && !s_above && v > threshold && nowMs - s_lastBeatMs > 300) {
    s_above = true;
    if (s_lastBeatMs) {
      const uint32_t ibi = nowMs - s_lastBeatMs;
      if (ibi >= 60000UL / HR_MAX_BPM && ibi <= 60000UL / HR_MIN_BPM) {
        s_ibi[s_ibiNext] = ibi;
        s_ibiNext = (s_ibiNext + 1) % 5;
        if (s_ibiCount < 5) ++s_ibiCount;
      }
    }
    s_lastBeatMs = nowMs;
  } else if (v < threshold) {
    s_above = false;
  }

  if (nowMs - s_lastBeatMs > 60000UL / HR_MIN_BPM * 2) {  // pulse lost
    s_ibiCount = 0;
    s_bpm = 0;
  } else if (s_ibiCount >= 3) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < s_ibiCount; ++i) sum += s_ibi[i];
    s_bpm = static_cast<uint8_t>(60000UL * s_ibiCount / sum);
  }
  return s_bpm;
}

#else
void heartRateInit() {}
uint8_t heartRateStep(uint32_t) { return 0; }
#endif
