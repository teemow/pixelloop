# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Reset the device and capture the boot log until PIXELLOOP-READY.

Exit 0 when READY arrived and no crash marker was seen, 1 otherwise. The full
log goes to --out (default artifacts/boot.log) and is echoed to stdout.
"""
from __future__ import annotations

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_ctl import READY, Device, scan_crashes  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--timeout", type=float, default=30.0)
    ap.add_argument("--out", default="artifacts/boot.log")
    ap.add_argument("--no-reset", action="store_true")
    args = ap.parse_args()

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w") as log:
        dev = Device(args.port, log=log, echo=True)
        if not args.no_reset:
            dev.reset()
        try:
            lines = dev.wait_for(READY, args.timeout)
        except TimeoutError as e:
            print(f"\nBOOT FAILED: {e} (log: {args.out})", file=sys.stderr)
            return 1
        finally:
            dev.close()
    crashes = scan_crashes(lines)
    if crashes:
        print("\nBOOT FAILED: crash markers in log:", file=sys.stderr)
        for c in crashes:
            print("  " + c, file=sys.stderr)
        return 1
    ready = [l for l in lines if READY in l][-1]
    print(f"\nBOOT OK: {ready} (log: {args.out})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
