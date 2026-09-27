# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow>=10", "numpy>=1.26"]
# ///
"""Compare two screenshots. Prints the fraction of differing pixels and writes
an optional diff image. Exit 0 when within tolerance.

  uv run tools/imgdiff.py a.png b.png [--tolerance 0.002] [--diff out.png]
"""
from __future__ import annotations

import argparse
import sys


def compare(a_path: str, b_path: str, diff_path: str | None = None, channel_slack: int = 8) -> float:
    import numpy as np
    from PIL import Image

    a = np.asarray(Image.open(a_path).convert("RGB")).astype(np.int16)
    b = np.asarray(Image.open(b_path).convert("RGB")).astype(np.int16)
    if a.shape != b.shape:
        raise ValueError(f"size mismatch {a.shape[1]}x{a.shape[0]} vs {b.shape[1]}x{b.shape[0]}")
    # RGB565 round trips wobble by a few counts; only count real differences.
    mask = (np.abs(a - b) > channel_slack).any(axis=-1)
    frac = float(mask.mean())
    if diff_path:
        out = (a // 3).astype(np.uint8)  # dimmed original
        out[mask] = (255, 0, 255)
        Image.fromarray(out, "RGB").save(diff_path)
    return frac


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--tolerance", type=float, default=0.002, help="max fraction of differing pixels")
    ap.add_argument("--diff", help="write a diff image here")
    args = ap.parse_args()
    frac = compare(args.a, args.b, args.diff)
    print(f"{frac:.4%} of pixels differ (tolerance {args.tolerance:.2%})")
    return 0 if frac <= args.tolerance else 1


if __name__ == "__main__":
    sys.exit(main())
