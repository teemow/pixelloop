#!/usr/bin/env bash
# Take one photo with the host webcam. Ground truth for "is the display really
# lit and showing what the framebuffer says". Point the camera at the board.
#   tools/photo.sh artifacts/desk.jpg [/dev/video0]
set -euo pipefail
out="${1:?output file}"; dev="${2:-${PIXELLOOP_CAMERA:-/dev/video0}}"
ffmpeg -hide_banner -loglevel error -y -f v4l2 -video_size 1280x720 -i "$dev" \
  -frames:v 8 -update 1 "$out"   # 8 frames: let auto-exposure settle, keep the last
echo "$out"
