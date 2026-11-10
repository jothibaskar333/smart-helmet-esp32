/*
 * heart_rate - OPTIONAL module (FEATURE_HEART_RATE, off by default).
 *
 * The circuit diagram (slide 15) includes an analog pulse sensor, but the
 * slides do not define its role. This module only measures BPM and reports
 * it over serial/BLE; no safety decision depends on it.
 *
 * Algorithm: adaptive threshold halfway between a decaying min and max,
 * a beat on each rising crossing (300 ms refractory period), and BPM from
 * the average of the last 5 inter-beat intervals.
 */
#pragma once

#include <Arduino.h>

void heartRateInit();
// Call every HR_PERIOD_MS. Returns BPM, or 0 while no valid pulse is found.
uint8_t heartRateStep(uint32_t nowMs);
