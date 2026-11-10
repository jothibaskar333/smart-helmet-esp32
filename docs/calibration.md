# Calibration

Every threshold in [`config.h`](../firmware/include/config.h) is a starting point. Values marked `[slide N]` came from our breadboard tests; values marked `[new]` haven't been validated on hardware yet. Tune them on the real helmet with the sensors in their final positions.

## Setup

1. Flash the firmware with `DEBUG_CSV 1` (the default).
2. Install the plotter's dependencies and find the serial port:
   ```bash
   pip install pyserial matplotlib
   python tools/serial_plotter.py --list-ports
   ```
3. Run the plotter with a preset (column set + threshold lines) for the module you're tuning:
   ```bash
   python tools/serial_plotter.py --port COM5 --preset crash
   ```
   Add `--log run1.csv` to save the session for later analysis. Close the serial monitor first, because only one program can hold the port.

CSV columns: `t_ms, ir, helmet, ign, dl_cm, dr_cm, acc_g, acc_peak_g, gyro_dps, pitch, pitch_base, nods, fsr, servo, hr, alerts` (`alerts` is the event-group bitmask: 1 crash, 2 drowsy, 4 blind L, 8 blind R, 16 grace, 32 fit fault, 64 impact).

Serial commands (type into the monitor and press Enter): `status`, `clear`, `csv on`, `csv off`.

## 1. Helmet detection (`--preset helmet`)

| Parameter | Default | Origin |
|-----------|---------|--------|
| `HELMET_ON_THRESHOLD` | 1500 | slide 23 |
| `HELMET_OFF_THRESHOLD` | 1300 | new |
| `HELMET_ON_DEBOUNCE_MS` / `OFF` | 1000 / 400 | new |

1. Record `ir` with the helmet **empty**, sitting on a table, for 30 s.
2. Record `ir` while **wearing** it: look around, talk, ride over bumps if you can.
3. Set `ON` about two-thirds of the way from the empty level to the lowest worn level, and `OFF` about one-third of the way.
4. Check that bright sunlight shining into the empty helmet doesn't cross `ON`. The feasibility slide flags false readings in bright light.

⚠ The Sharp GP2Y0A21 output **drops again below ~6 cm**. If the sensor sits right against the head, the reading may be *lower* when worn. Recess the sensor so the head is 6–15 cm away, or invert the logic.

## 2. Crash detection (`--preset crash`)

| Parameter | Default | Origin |
|-----------|---------|--------|
| `CRASH_IMPACT_G` | 4.0 g | new (slide: 35000 LSB ≈ 2.1 g) |
| `CRASH_SEVERE_G` | 8.0 g | new |
| `CRASH_IMPACT_MIN_SAMPLES` | 2 | new |
| `CRASH_STILL_*` | 2 s window, ±0.3 g, 40 °/s, 80 % | new |

`acc_peak_g` is the peak since the previous CSV row, so short spikes between rows aren't lost.

1. Wear the helmet and do normal things: nod hard, shake your head, put it on and take it off, drop into a seat, ride over potholes. Note the highest `acc_peak_g`. **`CRASH_IMPACT_G` must sit clearly above it** (we expect 2–3 g).
2. **Never test with a person in the helmet.** Drop the helmet onto a padded surface from 0.5–1 m with the IR sensor covered, so crash detection is armed. You should see `# EVT ... IMPACT 1` and then `CRASH 1` once the helmet lies still for 2 s.
3. Pick it up and move it within 2 s of a small drop: the event should end with `IMPACT 0` (treated as a bump).
4. Clear with `clear`, a 3 s BOOT press, or the dashboard button.

## 3. Drowsiness (`--preset drowsy`)

| Parameter | Default | Origin |
|-----------|---------|--------|
| `NOD_ANGLE_DEG` / `NOD_RELEASE_DEG` | 20° / 10° | new |
| `NOD_MIN_MS` | 250 ms | new |
| `NOD_COUNT_ALERT` in `NOD_WINDOW_MS` | 3 in 30 s | new |
| `HEAD_DROP_ALERT_MS` | 2000 ms | new |
| `INACTIVITY_MOVE_G` | 1.22 g | slide 25 (20000 LSB at ±2 g) |
| `INACTIVITY_ALERT_MS` | 6000 ms | slide 25 |

1. **Orientation check:** wear the helmet and nod forward. `pitch` must **increase**. If it decreases, set `IMU_PITCH_SIGN -1.0f`. If it barely changes, the board isn't mounted X-forward/Z-up; rotate it, or swap the axes in `imuStep()`.
2. Sit in your normal riding posture for 20 s. `pitch_base` should settle on your posture.
3. Glance down at an imaginary speedometer. The peak `pitch - pitch_base` must stay **below** `NOD_ANGLE_DEG`.
4. Do slow, sleepy nods. Each one should go clearly **above** `NOD_ANGLE_DEG` and increase the `nods` column.
5. The inactivity rule fires if the head is perfectly still for 6 s (for example at a red light). If that's too sensitive in practice, raise `INACTIVITY_ALERT_MS` or lower `INACTIVITY_MOVE_DPS`.

## 4. Blind spot (`--preset blind`)

| Parameter | Default | Origin |
|-----------|---------|--------|
| `BS_MIN_CM` | 10 cm | slides 22/26 |
| `BS_ALERT_CM` | 70 cm | slide 26 (slide 22: 80 cm) |
| `BS_MEDIAN_WINDOW` | 3 | new |

1. Hold a flat board at known distances (20, 50, 70, 100 cm) and check `dl_cm` / `dr_cm` (±2 cm is typical).
2. Wear the helmet: make sure your own shoulders or the bike don't show up within range. If they do, angle the sensors outwards or raise `BS_MIN_CM`.
3. A 70 cm range is short for traffic. On a moving bike, the HC-SR04's ~4 m maximum and its 15° beam are the practical limits (see feasibility). Test outdoors before raising `BS_ALERT_CM`.

## 5. Adaptive fit (`--preset fit`)

| Parameter | Default | Origin |
|-----------|---------|--------|
| `FIT_TARGET_MIN` / `MAX` | 1200 / 2000 | new |
| `FIT_VALID_MIN` / `MAX` | 50 / 3900 | new |
| `FIT_SERVO_LOOSE_DEG` / `TIGHT_DEG` | 30° / 120° | new |
| `FIT_STEP_DEG` | 2° | new |

1. **Set the angle limits first, with the strap mechanism attached and no head inside.** Find the loosest useful angle and the angle where the strap is snug on the smallest head you expect. These are hard limits, and the firmware will never go past `TIGHT_DEG`.
2. Wear the helmet, adjust the strap by hand to "comfortable" and to "too tight", and note `fsr` for each. Set the target band between them.
3. With the loop running, `servo` should move in 2° steps and stop inside the band. If it hunts back and forth, widen the band or reduce `FIT_STEP_DEG`.
4. Unplug the FSR: `fsr` drops below `FIT_VALID_MIN`, `alerts` shows bit 32, and the servo must **hold** its position.
