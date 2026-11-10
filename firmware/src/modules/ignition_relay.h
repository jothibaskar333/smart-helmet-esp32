/*
 * ignition_relay - fail-safe ignition interlock.
 *
 *   LOCKED  --helmet worn (debounced) and boot hold-off over-->  ENABLED
 *   ENABLED --helmet lost-->                                     GRACE
 *   GRACE   --helmet back-->                                     ENABLED
 *   GRACE   --IGNITION_GRACE_MS elapsed-->                       LOCKED
 *   any     --crash latched (CRASH_CUTS_IGNITION)-->             LOCKED
 *
 * The relay is driven OFF before the pin is even switched to output, so
 * the ignition is never enabled during boot.
 */
#pragma once

#include <Arduino.h>

#include "system_state.h"

void ignitionInit();
IgnitionState ignitionUpdate(bool helmetWorn, bool crashLatched, uint32_t nowMs);
const char* ignitionName(IgnitionState s);
