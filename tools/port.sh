#!/usr/bin/env bash
# Print the serial port of the attached ESP32-S3 (native USB-Serial/JTAG).
# Honours $PIXELLOOP_PORT, then /dev/serial/by-id, then /dev/ttyACM0.
set -euo pipefail
if [[ -n "${PIXELLOOP_PORT:-}" ]]; then echo "$PIXELLOOP_PORT"; exit 0; fi
for p in /dev/serial/by-id/*Espressif*; do
  [[ -e "$p" ]] && { readlink -f "$p"; exit 0; }
done
for p in /dev/ttyACM*; do
  [[ -e "$p" ]] && { echo "$p"; exit 0; }
done
echo "no ESP32 serial port found" >&2
exit 1
