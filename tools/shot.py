# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Take a screenshot of the device framebuffer and save it as PNG."""
from __future__ import annotations

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_ctl import Device  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--out", default="artifacts/shot.png")
    args = ap.parse_args()
    dev = Device(args.port)
    try:
        t0 = time.monotonic()
        w, h = dev.shot(args.out)
        print(f"{args.out}: {w}x{h} in {time.monotonic() - t0:.2f}s")
    finally:
        dev.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
