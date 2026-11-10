#!/usr/bin/env python3
"""Live serial plotter for threshold tuning.

Reads the CSV stream printed by the firmware (DEBUG_CSV=1) or by the example
sketches, and plots selected columns live.

Firmware stream format:
    H,t_ms,ir,helmet,...      header (re-sent every 10 s)
    D,12345,1620,1,...        data rows
    # ...                     log / event lines (printed, not plotted)
Example sketches print bare numbers ("42.1" or "42.1,77.0"); those are
plotted as value0, value1, ...

Usage:
    pip install pyserial matplotlib
    python serial_plotter.py --port COM5 --cols acc_g,acc_peak_g
    python serial_plotter.py --port /dev/ttyUSB0 --cols pitch,pitch_base --hline 20
    python serial_plotter.py --port COM5 --cols ir --hline 1500 --hline 1300 --log ir.csv
    python serial_plotter.py --list-ports
"""

from __future__ import annotations

import argparse
import collections
import sys
import threading
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover
    sys.exit("pyserial is required: pip install pyserial")

# Default column names, used until a header line arrives.
DEFAULT_HEADER = [
    "t_ms", "ir", "helmet", "ign", "dl_cm", "dr_cm", "acc_g", "acc_peak_g", "gyro_dps",
    "pitch", "pitch_base", "nods", "fsr", "servo", "hr", "alerts",
]

PRESETS = {
    "helmet": (["ir"], [1500, 1300]),
    "crash": (["acc_g", "acc_peak_g"], [4.0, 8.0]),
    "drowsy": (["pitch", "pitch_base"], []),
    "blind": (["dl_cm", "dr_cm"], [10, 70]),
    "fit": (["fsr", "servo"], [1200, 2000]),
}


class SerialReader(threading.Thread):
    """Background thread: parses lines into a rolling buffer per column."""

    def __init__(self, port: str, baud: int, window: int, log_path: str | None):
        super().__init__(daemon=True)
        self.ser = serial.Serial(port, baud, timeout=1)
        self.header = list(DEFAULT_HEADER)
        self.window = window
        # column name -> deque of (sample index, value)
        self.data: dict[str, collections.deque] = collections.defaultdict(
            lambda: collections.deque(maxlen=self.window))
        self.lock = threading.Lock()
        self.sample = 0
        self.log = open(log_path, "w", encoding="utf-8") if log_path else None
        self.running = True

    def run(self) -> None:
        while self.running:
            try:
                raw = self.ser.readline().decode("utf-8", errors="replace").strip()
            except serial.SerialException as exc:
                print(f"serial error: {exc}", file=sys.stderr)
                self.running = False
                break
            if raw:
                self.handle(raw)

    def handle(self, line: str) -> None:
        if line.startswith("#"):
            print(line)
            return
        parts = line.split(",")
        if parts[0] == "H":
            with self.lock:
                self.header = parts[1:]
            if self.log:
                self.log.write(",".join(self.header) + "\n")
            return
        if parts[0] == "D":
            names, values = self.header, parts[1:]
        else:  # bare numbers from the example sketches
            values = parts
            names = [f"value{i}" for i in range(len(values))]
        try:
            nums = [float(v) for v in values]
        except ValueError:
            print(line)  # not CSV, echo it
            return
        with self.lock:
            self.sample += 1
            for name, value in zip(names, nums):
                self.data[name].append((self.sample, value))
        if self.log:
            self.log.write(",".join(values) + "\n")

    def snapshot(self, cols: list[str]):
        with self.lock:
            series = {}
            for c in cols:
                points = list(self.data.get(c, []))
                series[c] = ([p[0] for p in points], [p[1] for p in points])
            return series

    def stop(self) -> None:
        self.running = False
        self.ser.close()
        if self.log:
            self.log.close()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--cols", default="", help="comma-separated column names to plot")
    ap.add_argument("--preset", choices=sorted(PRESETS), help="column/threshold preset per module")
    ap.add_argument("--hline", type=float, action="append", default=[], help="threshold line (repeatable)")
    ap.add_argument("--window", type=int, default=600, help="samples kept on screen")
    ap.add_argument("--log", help="also write the parsed CSV to this file")
    ap.add_argument("--list-ports", action="store_true")
    args = ap.parse_args()

    if args.list_ports:
        for p in serial.tools.list_ports.comports():
            print(f"{p.device}\t{p.description}")
        return 0
    if not args.port:
        ap.error("--port is required (use --list-ports to find it)")

    cols = [c for c in args.cols.split(",") if c]
    hlines = list(args.hline)
    if args.preset:
        pcols, plines = PRESETS[args.preset]
        cols = cols or pcols
        hlines = hlines or plines
    if not cols:
        cols = ["value0"]

    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    reader = SerialReader(args.port, args.baud, args.window, args.log)
    reader.start()

    fig, ax = plt.subplots(figsize=(10, 5))
    lines = {c: ax.plot([], [], label=c, linewidth=1.2)[0] for c in cols}
    for h in hlines:
        ax.axhline(h, linestyle="--", linewidth=0.8, color="grey")
    ax.set_xlabel("sample")
    ax.set_title(f"{args.port} @ {args.baud}")
    ax.legend(loc="upper left")
    ax.grid(alpha=0.3)

    def update(_frame):
        series = reader.snapshot(cols)
        lo, hi = None, None
        for c, (xs, ys) in series.items():
            lines[c].set_data(xs, ys)
            if ys:
                lo = min(ys) if lo is None else min(lo, min(ys))
                hi = max(ys) if hi is None else max(hi, max(ys))
        if lo is not None:
            for h in hlines:
                lo, hi = min(lo, h), max(hi, h)
            pad = (hi - lo) * 0.1 or 1.0
            ax.set_ylim(lo - pad, hi + pad)
            ax.set_xlim(max(0, reader.sample - args.window), max(args.window, reader.sample))
        return list(lines.values())

    anim = FuncAnimation(fig, update, interval=100, cache_frame_data=False)  # noqa: F841
    try:
        plt.show()
    finally:
        reader.stop()
        time.sleep(0.1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
