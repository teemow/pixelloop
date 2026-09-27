# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Reset the device and capture the console for a fixed time.

For firmware that does not speak the PixelLoop protocol (third-party images,
bring-up tests). Exit 1 if a crash marker shows up in the capture.
"""
from __future__ import annotations

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_ctl import Device, scan_crashes  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--out", default="artifacts/capture.log")
    ap.add_argument("--no-reset", action="store_true")
    args = ap.parse_args()
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    lines: list[str] = []
    with open(args.out, "w") as log:
        dev = Device(args.port, log=log, echo=True)
        if not args.no_reset:
            dev.reset()
        deadline = time.monotonic() + args.seconds
        try:
            while time.monotonic() < deadline:
                line = dev.readline(min(1.0, max(0.05, deadline - time.monotonic())))
                if line is not None:
                    lines.append(line)
        finally:
            dev.close()
    crashes = scan_crashes(lines)
    print(f"\n{len(lines)} lines in {args.seconds:.0f}s -> {args.out}")
    if crashes:
        print("crash markers:", file=sys.stderr)
        for c in crashes:
            print("  " + c, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
