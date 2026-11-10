/*
 * ble_service - one custom GATT service (NimBLE-Arduino).
 *
 * Service   6e3a0001-5a17-4c3e-9b8a-2f6d1e7a0001
 *   STATUS  ...0002  read + notify, 18-byte little-endian struct (below), every 500 ms
 *   ALERT   ...0003  read + notify, ASCII "NAME,value" on every event, e.g. "CRASH,1"
 *   COMMAND ...0004  write, ASCII: "CLEAR" clears a latched crash
 *
 * STATUS layout (packed, little-endian):
 *   0  u8   version (=1)
 *   1  u8   flags: b0 helmetOn, b1 ignition relay on, b2 grace, b3 crash,
 *               b4 drowsy, b5 blindL, b6 blindR, b7 fitFault
 *   2  u16  left distance, cm
 *   4  u16  right distance, cm
 *   6  i16  pitch relative to baseline, 0.1 deg
 *   8  u16  FSR raw (0-4095)
 *   10 u8   servo angle, deg
 *   11 u8   heart rate, bpm (0 = n/a)
 *   12 u16  IR raw (0-4095)
 *   14 u32  uptime, ms
 * 18 bytes fit in the default 20-byte notification payload (MTU 23), so no
 * MTU negotiation is needed.
 */
#pragma once

#include <Arduino.h>

#include "system_state.h"

#define BLE_SERVICE_UUID "6e3a0001-5a17-4c3e-9b8a-2f6d1e7a0001"
#define BLE_STATUS_UUID  "6e3a0002-5a17-4c3e-9b8a-2f6d1e7a0001"
#define BLE_ALERT_UUID   "6e3a0003-5a17-4c3e-9b8a-2f6d1e7a0001"
#define BLE_COMMAND_UUID "6e3a0004-5a17-4c3e-9b8a-2f6d1e7a0001"

void bleInit();
void bleSendStatus(const SystemState& s, uint32_t alertBits);
void bleSendEvent(const AppEvent& ev);
