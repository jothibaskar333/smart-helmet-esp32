/*
 * config.h - every pin, threshold, timing and feature flag in one place.
 *
 * Origin tags used in the comments:
 *   [slide N]  value taken from the original code sketch on presentation slide N
 *   [new]      value introduced for the integrated firmware
 * Every threshold is a starting point: CALIBRATE ON HARDWARE (docs/calibration.md).
 */
#pragma once

// =====================================================================
// Feature flags (1 = compiled in, 0 = compiled out)
// =====================================================================
#define FEATURE_BLE          1   // BLE status/alert service (NimBLE)
#define FEATURE_FIT_CONTROL  1   // FSR + servo auto-tension
#define FEATURE_HEART_RATE   0   // OPTIONAL: analog pulse sensor (on the circuit diagram, role not specified in slides)
#define FEATURE_DC_MOTOR     0   // OPTIONAL: MOSFET-driven DC motor used as a strong haptic (circuit diagram only)
#define DEBUG_CSV            1   // stream CSV over serial for tools/serial_plotter.py

// =====================================================================
// Pin map (ESP32 DevKit / esp32dev). See docs/hardware.md for the rationale.
// =====================================================================
// I2C (MPU-6050). Default ESP32 I2C pins.
#define PIN_I2C_SDA          21
#define PIN_I2C_SCL          22

// Ultrasonic HC-SR04. ECHO is 5 V: use a 1k/2k divider on both ECHO lines.
#define PIN_US_TRIG_L        5    // [slide 26] TRIG_L
#define PIN_US_ECHO_L        18   // [slide 26] ECHO_L
#define PIN_US_TRIG_R        19   // [slide 26] TRIG_R
#define PIN_US_ECHO_R        23   // [new] slide 26 used GPIO 21, which conflicts with I2C SDA

// Actuators (vibration motors through an NPN/MOSFET driver + flyback diode)
#define PIN_VIB_L            25   // [slide 26] VIB_L (also VIB in slides 22/24)
#define PIN_VIB_R            26   // [slide 26] VIB_R
#define PIN_BUZZER           27   // [slide 24/25] BUZZER (active buzzer)
#define PIN_LED_WARN         4    // [new] blind-spot / crash warning LED (330 ohm)
#define PIN_LED_STATUS       2    // [new] on-board LED: heartbeat / BLE connected

// Ignition interlock relay
#define PIN_RELAY            14   // [slide 23] RELAY_PIN (see boot-glitch note in docs/hardware.md)
#define RELAY_ACTIVE_LOW     0    // set 1 for opto relay modules that energise on LOW input

// Analog inputs: ADC1 only (GPIO 32-39). ADC2 is unusable while BLE/Wi-Fi is active.
#define PIN_HELMET_IR        34   // [slide 23] HELMET_SENSOR (Sharp IR, analog)
#define PIN_FSR              35   // [new] FSR 402 / Velostat divider (10k to GND)
#define PIN_HEART_RATE       32   // [new] optional pulse sensor

// Servo + optional DC motor
#define PIN_SERVO            13   // [new] fit-control servo signal (servo on its own 5-6 V supply)
#define PIN_DC_MOTOR         33   // [new] optional logic-level MOSFET gate

// Crash-clear button: on-board BOOT button (active LOW, internal pull-up)
#define PIN_CLEAR_BUTTON     0
#define CLEAR_LONG_PRESS_MS  3000 // [new] hold BOOT for 3 s to clear a latched crash

// =====================================================================
// Helmet detection + ignition interlock
// =====================================================================
#define HELMET_PERIOD_MS         200   // [slide 23] loop delay(200)
#define HELMET_ADC_SAMPLES       8     // [new] samples averaged per reading
#define HELMET_ON_THRESHOLD      1500  // [slide 23] analogRead > 1500 means head present
#define HELMET_OFF_THRESHOLD     1300  // [new] hysteresis: must fall below this to count as removed
#define HELMET_ON_DEBOUNCE_MS    1000  // [new] head must be detected continuously this long
#define HELMET_OFF_DEBOUNCE_MS   400   // [new] reading must stay low this long before "removed"
#define IGNITION_GRACE_MS        5000  // [new] ignition stays on this long after the helmet is lost
#define IGNITION_BOOT_HOLDOFF_MS 2000  // [new] relay is never energised in the first 2 s after boot
#define CRASH_CUTS_IGNITION      1     // [new] a latched crash locks the ignition until cleared

// =====================================================================
// IMU (MPU-6050): sampling, crash detection, drowsiness
// =====================================================================
#define IMU_PERIOD_MS            10    // [new] 100 Hz. Slide 24 used 80 ms, which can miss 10-30 ms impact pulses
#define IMU_ACCEL_LSB_PER_G      2048.0f  // +/-16 g range (set explicitly in imuInit)
#define IMU_GYRO_LSB_PER_DPS     65.5f    // +/-500 deg/s range
#define IMU_COMP_ALPHA           0.98f // [new] complementary filter weight on the gyro
#define IMU_PITCH_SIGN           1.0f  // [new] flip to -1 if nodding forward shows negative pitch
// Mounting assumption: X axis points forward, Z axis up. See docs/calibration.md.

// Crash: impact spike, then confirmation
#define CRASH_IMPACT_G           4.0f  // [new] slide 24 used 35000 LSB at +/-2 g, about 2.1 g (too sensitive for potholes)
#define CRASH_IMPACT_MIN_SAMPLES 2     // [new] consecutive samples above threshold (>= 20 ms) to count as impact
#define CRASH_SEVERE_G           8.0f  // [new] an impact this hard latches immediately, no stillness check
#define CRASH_SETTLE_MS          500   // [new] ignore motion just after the impact (tumbling)
#define CRASH_STILL_WINDOW_MS    2000  // [new] the rider/helmet must then stay still this long to confirm
#define CRASH_STILL_ACC_TOL_G    0.30f // [new] |accel - 1 g| tolerance counted as "still"
#define CRASH_STILL_GYRO_DPS     40.0f // [new] gyro norm below this counts as "still"
#define CRASH_STILL_MIN_RATIO    0.80f // [new] fraction of samples in the window that must be still

// Drowsiness
#define DROWSY_PERIOD_MS         120   // [slide 25] loop delay(120)
#define NOD_ANGLE_DEG            20.0f // [new] forward pitch beyond baseline that counts as a nod
#define NOD_RELEASE_DEG          10.0f // [new] pitch must return below this to end the nod (hysteresis)
#define NOD_MIN_MS               250   // [new] a nod must last at least this long (ignores quick glances)
#define NOD_COUNT_ALERT          3     // [new] this many nods ...
#define NOD_WINDOW_MS            30000 // [new] ... within this window raises a drowsiness alert
#define HEAD_DROP_ALERT_MS       2000  // [new] head held past NOD_ANGLE_DEG this long alerts immediately
#define PITCH_BASELINE_TAU_S     20.0f // [new] slow baseline tracks the rider's normal head posture
#define INACTIVITY_MOVE_G        1.22f // [slide 25] magnitude > 20000 LSB at +/-2 g = 1.22 g counts as movement
#define INACTIVITY_MOVE_DPS      30.0f // [new] gyro norm above this also counts as movement
#define INACTIVITY_ALERT_MS      6000  // [slide 25] alarm after 6000 ms without movement
#define DROWSY_ALERT_HOLD_MS     3000  // [new] minimum duration of a drowsiness alert

// =====================================================================
// Blind-spot ultrasonic
// =====================================================================
#define BS_PERIOD_MS             150   // [slide 22/26] loop delay(150)
#define BS_MIN_CM                10.0f // [slide 22/26] readings below this are ignored (noise / own body)
#define BS_ALERT_CM              70.0f // [slide 26] dual-side alert range 10-70 cm (slide 22 single used 80)
#define BS_ECHO_TIMEOUT_MS       25    // [new] ~4 m max range. Slide used pulseIn timeout 30000 us
#define BS_NO_ECHO_CM            400.0f// [new] value reported when no echo is received
#define BS_MEDIAN_WINDOW         3     // [new] rolling median over the last N readings per side
#define BS_INTER_SENSOR_GAP_MS   20    // [new] gap between left and right pings to avoid cross-talk

// =====================================================================
// Adaptive fit (FSR + servo)
// =====================================================================
#define FIT_PERIOD_MS            250   // [new]
#define FIT_ADC_SAMPLES          8     // [new]
#define FIT_TARGET_MIN           1200  // [new] FSR ADC band considered a correct fit
#define FIT_TARGET_MAX           2000  // [new]
#define FIT_VALID_MIN            50    // [new] below: sensor open / not touching -> hold position
#define FIT_VALID_MAX            3900  // [new] above: saturated or shorted -> hold position
#define FIT_STEP_DEG             2     // [new] servo moves at most this much per period
#define FIT_SERVO_LOOSE_DEG      30    // [new] hard limit: fully loose (also the start/release angle)
#define FIT_SERVO_TIGHT_DEG      120   // [new] hard limit: the servo can never tighten beyond this
#define FIT_RELEASE_ON_CRASH     1     // [new] loosen the strap after a confirmed crash ("emergency release", slide 7)
#define SERVO_MIN_US             500   // MG995 / SG90 pulse range
#define SERVO_MAX_US             2400

// =====================================================================
// Alerts / BLE / console
// =====================================================================
#define ALERT_TICK_MS            20    // [new] pattern generator resolution
#define BLE_DEVICE_NAME          "SmartHelmet"
#define BLE_STATUS_PERIOD_MS     500   // [new] status notify rate
#define CONSOLE_PERIOD_MS        100   // [new] CSV rate (10 Hz) + serial command polling
#define EVENT_QUEUE_LEN          16

// Optional modules
#define HR_PERIOD_MS             20    // [new] 50 Hz sampling of the pulse sensor
#define HR_MIN_BPM               40
#define HR_MAX_BPM               200
#define DC_MOTOR_ALERT_DUTY      0.8f  // [new] duty (0-1) while a crash/drowsy alert is active

// =====================================================================
// FreeRTOS task priorities (higher = more urgent). Arduino loopTask is 1.
// Order from the spec: crash > helmet/ignition > blind spot > drowsiness > fit > BLE
// =====================================================================
#define PRIO_IMU_CRASH           6
#define PRIO_ALERTS              5
#define PRIO_HELMET_IGNITION     5
#define PRIO_BLIND_SPOT          4
#define PRIO_DROWSINESS          3
#define PRIO_FIT                 2
#define PRIO_HEART_RATE          2
#define PRIO_BLE                 1
#define PRIO_CONSOLE             1
#define APP_CORE                 1   // sensor tasks run on core 1; the NimBLE host runs on core 0
