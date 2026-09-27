# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Identify the attached board from the probe firmware's report.

Run `make probe` first (builds + flashes firmware/src/apps/probe), then this
tool resets the device, captures the PROBE-BEGIN..PROBE-END block, matches the
I2C fingerprint against the known Waveshare ESP32-S3 Touch LCD boards and
writes artifacts/probe.json for the next step.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_ctl import Device  # noqa: E402

# (sda, scl) -> {addr: chip}. Only pins that are I2C on at least one board are
# probed by the firmware; the mapping here turns addresses into names.
KNOWN_CHIPS = {
    0x15: "CST816x touch",
    0x1A: "CST328 touch",
    0x53: "SPD2010 touch",
    0x5D: "GT911 touch",
    0x14: "GT911 touch (alt)",
    0x6B: "QMI8658 IMU",
    0x6A: "QMI8658 IMU (alt)",
    0x51: "PCF85063 RTC",
    0x20: "TCA9554 IO expander",
    0x24: "CH422G IO expander",
    0x38: "FT6x36 touch / CH422G",
    0x18: "ES8311 codec",
    0x40: "ES7210 ADC",
    0x7E: "unknown (Waveshare 2.8 reserved)",
}

# Fingerprints: short board id (see boards/README.md) -> required addresses per (sda, scl) bus.
BOARDS = {
    "ws169": {
        "name": "Waveshare ESP32-S3-Touch-LCD-1.69",
        "display": "ST7789V2 240x280 SPI", "touch": "CST816D/T",
        "buses": [((11, 10), {0x15, 0x6B, 0x51})],
        "reject": {0x20},
        "url": "https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69",
    },
    "ws185": {
        "name": "Waveshare ESP32-S3-Touch-LCD-1.85",
        "display": "ST77916 360x360 QSPI", "touch": "CST816",
        "buses": [((11, 10), {0x6B, 0x51, 0x20}), ((1, 3), {0x15})],
        "url": "https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85",
    },
    "ws146": {
        "name": "Waveshare ESP32-S3-Touch-LCD-1.46",
        "display": "SPD2010 412x412 QSPI", "touch": "SPD2010",
        "buses": [((11, 10), {0x53, 0x6B, 0x51, 0x20})],
        "url": "https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46",
    },
    "ws28": {
        "name": "Waveshare ESP32-S3-Touch-LCD-2.8",
        "display": "ST7789 240x320 SPI", "touch": "CST328",
        "buses": [((11, 10), {0x6B, 0x51}), ((1, 3), {0x1A})],
        "url": "https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8",
    },
    "ws21": {
        "name": "Waveshare ESP32-S3-Touch-LCD-2.1",
        "display": "ST7701 480x480 RGB", "touch": "CST820",
        "buses": [((15, 7), {0x15, 0x6B, 0x51, 0x20})],
        "url": "https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.1",
    },
    "ws43": {
        "name": "Waveshare ESP32-S3-Touch-LCD-4.3",
        "display": "ST7262 800x480 RGB", "touch": "GT911",
        "buses": [((8, 9), {0x5D})],
        "url": "https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3",
    },
    "ws128": {
        "name": "Waveshare ESP32-S3-Touch-LCD-1.28",
        "display": "GC9A01 240x240 SPI", "touch": "CST816S",
        "buses": [((6, 7), {0x15})],
        "url": "https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28",
    },
}


def match(scan: dict[tuple[int, int], set[int]]) -> list[tuple[str, float]]:
    """Score every known board against the scan; 1.0 = all required addresses seen."""
    scored = []
    for name, spec in BOARDS.items():
        need = sum(len(a) for _, a in spec["buses"])
        hit = 0
        for (sda, scl), addrs in spec["buses"]:
            hit += len(addrs & scan.get((sda, scl), set()))
        seen_all = set().union(*scan.values()) if scan else set()
        if spec.get("reject", set()) & seen_all:
            hit = 0
        scored.append((name, hit / need if need else 0.0))
    return sorted(scored, key=lambda t: -t[1])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--timeout", type=float, default=20.0)
    ap.add_argument("--out", default="artifacts/probe.json")
    ap.add_argument("--no-reset", action="store_true")
    args = ap.parse_args()

    dev = Device(args.port, echo=True)
    port = dev.port
    if not args.no_reset:
        dev.reset()

    report: dict = {}
    scan: dict[tuple[int, int], set[int]] = {}
    in_block = False
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        line = dev.readline(1.0)
        if line is None:
            continue
        if "PROBE-END" in line and in_block:
            break
        if "PROBE-BEGIN" in line:
            in_block = True
            continue
        if not in_block or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        if rec.get("k") == "i2c":
            addrs = set(rec["addrs"])
            # A real bus has a handful of devices. Dozens of ACKs means the
            # pins are something else (LCD SPI lines on the 1.69) and the
            # open-drain probe is reading noise.
            scan[(rec["sda"], rec["scl"])] = addrs if len(addrs) <= 12 else set()
        else:
            report.update(rec)
    dev.close()

    if not in_block:
        print("no PROBE-BEGIN seen: is the probe firmware flashed? (make probe)", file=sys.stderr)
        return 2

    ranking = match(scan)
    best, score = ranking[0] if ranking else ("unknown", 0.0)
    result = {
        "port": port,
        "captured_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "chip": report,
        "i2c": [
            {"sda": sda, "scl": scl, "addrs": sorted(a), "chips": [KNOWN_CHIPS.get(x, "?") for x in sorted(a)]}
            for (sda, scl), a in sorted(scan.items()) if a
        ],
        "board": best if score >= 0.99 else None,
        "board_name": BOARDS[best]["name"] if score >= 0.99 else None,
        "candidates": [{"board": n, "score": round(s, 2)} for n, s in ranking[:3]],
    }
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w") as f:
        json.dump(result, f, indent=2)

    print()
    print(f"chip     : {report.get('model')} rev {report.get('rev')}  flash {report.get('flash_mb')} MB  psram {report.get('psram_mb')} MB  mac {report.get('mac')}")
    for bus in result["i2c"]:
        found = ", ".join(f"0x{a:02X} {c}" for a, c in zip(bus["addrs"], bus["chips"]))
        print(f"i2c {bus['sda']:>2}/{bus['scl']:<2}: {found}")
    if result["board"]:
        print(f"board    : {best}  {BOARDS[best]['name']} ({BOARDS[best]['display']}, touch {BOARDS[best]['touch']})")
        print(f"docs     : {BOARDS[best]['url']}")
    else:
        print(f"board    : UNKNOWN (best guess {best} at {score:.0%}) - extend BOARDS in tools/probe.py")
    print(f"written  : {args.out}")
    return 0 if result["board"] else 1


if __name__ == "__main__":
    sys.exit(main())
