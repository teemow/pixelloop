# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Replay a drive script against the device.

Script lines (one per line, '#' starts a comment):
  reset                       hard-reset the device
  expect-log <text> [timeout] wait until a console line contains <text>
  wait <ms>                   sleep
  tap <x> <y>                 synthetic touch
  swipe <x1> <y1> <x2> <y2> [ms]
  shot <name>                 screenshot -> <out-dir>/<name>.png
  stats                       print the STATS json
  send <raw line>             any other console command, waits for OK

Exit 0 when every step succeeded. Everything read from the console is
appended to <out-dir>/drive.log.
"""
from __future__ import annotations

import argparse
import os
import shlex
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_ctl import Device  # noqa: E402


def run_script(dev: Device, path: str, out_dir: str) -> list[str]:
    shots: list[str] = []
    with open(path) as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            argv = shlex.split(line)
            op, args = argv[0], argv[1:]
            t0 = time.monotonic()
            if op == "reset":
                dev.reset()
            elif op == "expect-log":
                dev.wait_for(args[0], float(args[1]) if len(args) > 1 else 30.0)
            elif op == "wait":
                time.sleep(int(args[0]) / 1000.0)
            elif op == "tap":
                dev.tap(int(args[0]), int(args[1]))
            elif op == "swipe":
                dev.swipe(*(int(a) for a in args[:4]), int(args[4]) if len(args) > 4 else 250)
            elif op == "shot":
                out = os.path.join(out_dir, f"{args[0]}.png")
                w, h = dev.shot(out)
                shots.append(out)
                print(f"  shot {out} ({w}x{h})")
            elif op == "stats":
                print(f"  stats {dev.stats()}")
            elif op == "send":
                dev.cmd(" ".join(args))
            else:
                raise ValueError(f"{path}:{lineno}: unknown op {op!r}")
            print(f"{path}:{lineno}: {line}  [{time.monotonic() - t0:.2f}s]")
    return shots


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--script", required=True)
    ap.add_argument("--out-dir", default="artifacts")
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    with open(os.path.join(args.out_dir, "drive.log"), "a") as log:
        dev = Device(args.port, log=log)
        try:
            run_script(dev, args.script, args.out_dir)
        except Exception as e:  # noqa: BLE001 - report and fail the step
            print(f"DRIVE FAILED: {e}", file=sys.stderr)
            return 1
        finally:
            dev.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
