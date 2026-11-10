/*
 * helmet_detect - is a head inside the helmet?
 *
 * Reads the analog Sharp IR proximity sensor (slide 23: analogRead > 1500)
 * and adds averaging, a two-threshold hysteresis band and time debouncing,
 * so the "worn" output does not flicker around the threshold.
 */
#pragma once

#include <Arduino.h>

struct HelmetReading {
  uint16_t raw;   // averaged ADC value (0-4095)
  bool worn;      // debounced result
};

void helmetDetectInit();
HelmetReading helmetDetectUpdate(uint32_t nowMs);
