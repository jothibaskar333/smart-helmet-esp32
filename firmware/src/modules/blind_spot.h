/*
 * blind_spot - left/right HC-SR04 blind-spot monitoring (slides 22 and 26).
 *
 * Changes vs. the slide sketches:
 *   - Echo pulses are timed by a GPIO interrupt (esp_timer microseconds)
 *     instead of busy-waiting in pulseIn(). Higher-priority tasks can
 *     preempt this task without distorting the measurement, and the task
 *     sleeps on a FreeRTOS notification while waiting for the echo.
 *   - Each side keeps a rolling median of the last BS_MEDIAN_WINDOW readings.
 *   - Sides are pinged one after another with a gap, to avoid cross-talk.
 * Alert rule (slide 26): BS_MIN_CM < distance < BS_ALERT_CM.
 */
#pragma once

#include <Arduino.h>

struct BlindSpotReading {
  float leftCm;   // median distance, BS_NO_ECHO_CM if nothing in range
  float rightCm;
  bool alertLeft;
  bool alertRight;
};

void blindSpotInit();
// Must be called from the task that owns the sensors (it blocks on notifications).
BlindSpotReading blindSpotStep();
