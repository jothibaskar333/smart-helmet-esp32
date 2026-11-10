#include "ignition_relay.h"

#include "config.h"

static IgnitionState s_state = IGN_LOCKED;
static uint32_t s_graceStartMs = 0;

static void relayWrite(bool energise) {
  const bool level = RELAY_ACTIVE_LOW ? !energise : energise;
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
}

void ignitionInit() {
  // Set the output latch to "off" first, then enable the driver, so the pin
  // never drives the "on" level. (GPIO 14 still toggles briefly during the
  // ROM bootloader, before this runs: see docs/hardware.md.)
  relayWrite(false);
  pinMode(PIN_RELAY, OUTPUT);
  relayWrite(false);
  s_state = IGN_LOCKED;
}

IgnitionState ignitionUpdate(bool helmetWorn, bool crashLatched, uint32_t nowMs) {
  IgnitionState next = s_state;

  if (CRASH_CUTS_IGNITION && crashLatched) {
    next = IGN_LOCKED;
  } else if (nowMs < IGNITION_BOOT_HOLDOFF_MS) {
    next = IGN_LOCKED;
  } else {
    switch (s_state) {
      case IGN_LOCKED:
        if (helmetWorn) next = IGN_ENABLED;
        break;
      case IGN_ENABLED:
        if (!helmetWorn) {
          next = IGN_GRACE;
          s_graceStartMs = nowMs;
        }
        break;
      case IGN_GRACE:
        if (helmetWorn) {
          next = IGN_ENABLED;
        } else if (nowMs - s_graceStartMs >= IGNITION_GRACE_MS) {
          next = IGN_LOCKED;
        }
        break;
    }
  }

  if (next != s_state) {
    s_state = next;
    eventPost(EVT_IGNITION, s_state);
  }
  relayWrite(s_state != IGN_LOCKED);
  // The EVT_IGNITION event above already reports GRACE, so set the bit quietly.
  if (s_state == IGN_GRACE) {
    xEventGroupSetBits(alertGroup(), ALERT_IGN_GRACE);
  } else {
    xEventGroupClearBits(alertGroup(), ALERT_IGN_GRACE);
  }
  return s_state;
}

const char* ignitionName(IgnitionState s) {
  switch (s) {
    case IGN_LOCKED:  return "LOCKED";
    case IGN_ENABLED: return "ENABLED";
    case IGN_GRACE:   return "GRACE";
  }
  return "?";
}
