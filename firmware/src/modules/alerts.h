/*
 * alerts - turns the alert event-group bits into actuator patterns.
 *
 * Priority (highest first):
 *   CRASH      buzzer 4 Hz beeps, both vibration motors on, warning LED blinking
 *   DROWSY     both motors pulsing 0.5 s on/off, buzzer short chirps
 *   IMPACT     single buzzer chirp each second while a crash is being confirmed
 *   BLIND L/R  motor on the matching side steady, warning LED steady
 *   IGN GRACE  short buzzer chirp every second (helmet removed while ignition on)
 * Status LED: slow heartbeat, steady when a BLE client is connected.
 */
#pragma once

#include <Arduino.h>

void alertsInit();
// Called every ALERT_TICK_MS by the alerts task.
void alertsStep(uint32_t nowMs, uint32_t bits, bool bleConnected);
