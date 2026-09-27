# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Run every drive script under tests/ and compare its screenshots with the
goldens in tests/golden/<board>/.

  uv run tools/test.py --board ws169 [--update] [--only smoke]

A screenshot without a golden is reported as NEW and (with --update) copied
into place. Exit 0 when all scripts ran and every compared image is within
tolerance.
"""
from __future__ import annotations

import argparse
import glob
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from drive import run_script  # noqa: E402
from imgdiff import compare  # noqa: E402
from serial_ctl import Device  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--board", required=True)
    ap.add_argument("--tests-dir", default="tests")
    ap.add_argument("--out-dir", default="artifacts")
    ap.add_argument("--tolerance", type=float, default=0.002)
    ap.add_argument("--update", action="store_true", help="accept current screenshots as goldens")
    ap.add_argument("--only", help="run only the script with this name (without .txt)")
    args = ap.parse_args()

    scripts = sorted(glob.glob(os.path.join(args.tests_dir, "*.txt")))
    if args.only:
        scripts = [s for s in scripts if os.path.splitext(os.path.basename(s))[0] == args.only]
    if not scripts:
        print("no test scripts found", file=sys.stderr)
        return 1
    golden_dir = os.path.join(args.tests_dir, "golden", args.board)
    os.makedirs(golden_dir, exist_ok=True)

    failures = 0
    with open(os.path.join(args.out_dir, "test.log"), "a") as log:
        dev = Device(args.port, log=log)
        try:
            for script in scripts:
                name = os.path.splitext(os.path.basename(script))[0]
                run_dir = os.path.join(args.out_dir, "tests", name)
                os.makedirs(run_dir, exist_ok=True)
                print(f"== {name}")
                try:
                    shots = run_script(dev, script, run_dir)
                except Exception as e:  # noqa: BLE001
                    print(f"FAIL {name}: {e}")
                    failures += 1
                    continue
                for shot in shots:
                    base = os.path.basename(shot)
                    golden = os.path.join(golden_dir, f"{name}--{base}")
                    if not os.path.exists(golden):
                        if args.update:
                            shutil.copy(shot, golden)
                            print(f"  NEW  {golden} (accepted)")
                        else:
                            print(f"  NEW  {base}: no golden at {golden}; inspect it and run with --update")
                            failures += 1
                        continue
                    diff_path = os.path.join(run_dir, f"diff--{base}")
                    frac = compare(golden, shot, diff_path)
                    if frac <= args.tolerance:
                        print(f"  PASS {base} ({frac:.3%} differ)")
                    elif args.update:
                        shutil.copy(shot, golden)
                        print(f"  UPD  {base} ({frac:.3%} differ, golden replaced)")
                    else:
                        print(f"  FAIL {base} ({frac:.3%} differ > {args.tolerance:.2%}); diff: {diff_path}")
                        failures += 1
        finally:
            dev.close()
    print(f"\n{'OK' if failures == 0 else 'FAILED'}: {len(scripts)} script(s), {failures} failure(s)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
