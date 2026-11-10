# Example sketches (from the presentation)

These are the five initial code sketches from slides 22–26 of the [project presentation](../docs/presentation.pdf), reproduced **exactly** with the same pins, thresholds, and loop delays. Only header comments were added. Each one exercises a single safety function and was tested on our breadboard prototype.

They are deliberately minimal. The [integrated firmware](../firmware) combines all of them under FreeRTOS and fixes the issues noted below.

| # | Sketch | Slide | What it does |
|---|--------|-------|--------------|
| 01 | [01_blind_spot_single](01_blind_spot_single/01_blind_spot_single.ino) | 22 | One ultrasonic sensor; vibrates when an object is 10–80 cm away |
| 02 | [02_helmet_detect_relay](02_helmet_detect_relay/02_helmet_detect_relay.ino) | 23 | IR proximity sensor > 1500 → ignition relay ON |
| 03 | [03_crash_detection](03_crash_detection/03_crash_detection.ino) | 24 | IMU acceleration magnitude > 35000 → buzzer + vibration |
| 04 | [04_drowsiness_detection](04_drowsiness_detection/04_drowsiness_detection.ino) | 25 | No head movement for 6 s → buzzer |
| 05 | [05_blind_spot_dual](05_blind_spot_dual/05_blind_spot_dual.ino) | 26 | Left/right ultrasonic sensors, per-side vibration at 10–70 cm |

## Wiring

| Signal | 01 | 02 | 03 | 04 | 05 |
|--------|----|----|----|----|----|
| HC-SR04 TRIG (L) | GPIO 5 | | | | GPIO 5 |
| HC-SR04 ECHO (L) | GPIO 18 | | | | GPIO 18 |
| HC-SR04 TRIG (R) | | | | | GPIO 19 |
| HC-SR04 ECHO (R) | | | | | GPIO 21 ⚠ |
| Vibration motor (L) | GPIO 25 | | GPIO 25 | | GPIO 25 |
| Vibration motor (R) | | | | | GPIO 26 |
| Buzzer | | | GPIO 27 | GPIO 27 | |
| IR proximity (analog) | | GPIO 34 | | | |
| Relay IN | | GPIO 14 | | | |
| MPU-6050 SDA / SCL | | | GPIO 21 / 22 | GPIO 21 / 22 | |

Electrical notes (they apply to every sketch, see [docs/hardware.md](../docs/hardware.md)):
- The HC-SR04 ECHO pin outputs **5 V**. Put a 1 kΩ / 2 kΩ divider in front of each ESP32 input.
- Drive the vibration motors through an NPN transistor or logic-level MOSFET with a flyback diode, not straight from a GPIO pin.
- ⚠ In sketch 05, ECHO_R on GPIO 21 is fine on its own, but GPIO 21 is the I²C SDA line the MPU-6050 needs. The integrated firmware moves ECHO_R to **GPIO 23**.

## Expected serial output (115200 baud)

| Sketch | Output | Typical values |
|--------|--------|----------------|
| 01 | one distance per line, every 150 ms | `57.24`; `0.00` when there is no echo (timeout) |
| 02 | `1` / `0`, every 200 ms | `1` with a head (or hand) close to the sensor |
| 03 | acceleration magnitude in raw LSB, every 80 ms | ≈ `16384` at rest (1 g at ±2 g range); > `35000` on a hard knock |
| 04 | acceleration magnitude in raw LSB, every 120 ms | ≈ `16384` at rest; the buzzer sounds after 6 s without a > `20000` spike |
| 05 | `left,right` distances, every 150 ms | `45.10,0.00` |

You can plot any of them live with `python tools/serial_plotter.py --port COM5 --cols value0,value1`.

## Building

**Arduino IDE:** install the *esp32* board package (Espressif), select **ESP32 Dev Module**, and for 03/04 install the **MPU6050** library by **Electronic Cats** from the Library Manager. Then open the `.ino` file and upload.

**PlatformIO CI** (from the repository root):

```bash
pio ci examples/01_blind_spot_single/01_blind_spot_single.ino --board esp32dev \
       -O "platform=espressif32@6.12.0"
pio ci examples/03_crash_detection/03_crash_detection.ino --board esp32dev \
       -O "platform=espressif32@6.12.0" -O "lib_deps=electroniccats/MPU6050@1.4.4"
```

All five sketches compile cleanly with `-Wall` against espressif32 6.12.0 (Arduino-ESP32 2.0.17).

## Known limitations (fixed in the integrated firmware)

| Sketch | Limitation | Integrated firmware |
|--------|-----------|---------------------|
| 01, 05 | `pulseIn()` busy-waits up to 30 ms per sensor; single noisy readings toggle the motor | Interrupt-timed echo + rolling median of 3 |
| 02 | No debounce: the relay chatters around the threshold, and a brief dropout cuts the ignition | Hysteresis (1500 / 1300), 1 s on-debounce, 5 s grace period |
| 03 | Raw LSB at the default ±2 g range (saturates at 2 g); `ax*ax+…` int overflow above ~2.8 g; single-sample trigger; no latch | Explicit ±16 g range, float math in g, impact + stillness confirmation, latched until cleared |
| 04 | Inactivity is the only signal | Pitch-based nod / head-drop detection with a complementary filter, plus the inactivity rule |
| 05 | GPIO 21 conflicts with I²C SDA | ECHO_R → GPIO 23 |
