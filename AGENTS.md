# PixelLoop — agent playbook

You are working on firmware for an ESP32-S3 display board that is plugged into
this machine over USB. You can build, flash, drive the UI and take screenshots
without a human touching the device. This file tells you how. `CLAUDE.md` is a
symlink to it.

## The loop

```
edit code  →  make flash  →  make run (boot log)  →  make shot / make drive  →  look at the PNG  →  iterate
```

1. **Identify the board once per device:** `make probe`. It flashes the probe
   firmware, scans the I2C buses and writes `artifacts/probe.json` with the
   board id (`ws169`, `ws185`, ...). Later targets read `BOARD` from that file.
2. **Build + flash:** `make flash` (or `make build` to only compile). Build
   errors come back on stderr; the ESP-IDF exception decoder symbolises
   crashes in the monitor.
3. **Boot check:** `make run` flashes, resets and captures the console into
   `artifacts/boot.log` until the firmware prints `PIXELLOOP-READY` (or times
   out). A boot log containing `Guru Meditation`, `abort()`, `assert failed`
   or a watchdog reset is a failed iteration — read it, fix it, go again.
4. **See the screen:** `make shot NAME=<what>` asks the firmware for its
   framebuffer over the console and writes `artifacts/<what>.png`. Open the PNG
   and look at it. That is your eyes.
5. **Drive the UI:** `make drive SCRIPT=tests/<name>.txt` replays a script of
   `tap x y`, `swipe x1 y1 x2 y2 [ms]`, `wait ms`, `shot name`, `expect-log
   <text>` lines. Scripts run identically against the real device and the
   desktop simulator, so write them once.
6. **Regression:** `make test` runs every `tests/*.txt`, compares screenshots
   against `tests/golden/<board>/*.png` (per-pixel tolerance) and fails on
   drift. `make test UPDATE=1` accepts the current screenshots as goldens; do
   that only after you have looked at the new PNGs and they are right.
   Anything time-dependent on screen must be pinned by the script first
   (`send clock 43200`), otherwise the golden flakes.

Ground truth beyond the framebuffer: `make photo NAME=desk` takes a webcam
picture of the physical board (point the camera at it). Use it when the
framebuffer says one thing and you suspect the panel shows another (wrong
orientation, backlight off, colour swap).

## Rules

- **Never run two console clients at once.** Every Makefile target takes
  `artifacts/.port.lock`. If you call `pio` or `esptool` directly, take the lock
  yourself: `flock artifacts/.port.lock <cmd>`.
- **One flash per change, then look.** Do not stack several edits before the
  first screenshot; the loop is cheap (a rebuild is seconds, a flash ~10 s).
- **Never leave the device in the bootloader.** If a flash aborts, `make run`
  again; the tools always end with a hard reset into the app.
- **Artifacts are evidence, not source.** `artifacts/` is git-ignored; goldens
  under `tests/golden/` are the only committed images. Reference the artifact
  file names in your commit message / PR so a reviewer knows what you saw.
- **Board data comes from `boards/`.** Never hard-code a GPIO in an app; include
  `board.h` and use its `BOARD_*` macros. Adding a board = a new directory under
  `boards/`, an `[env:<id>]` in `firmware/platformio.ini`, a fingerprint in
  `tools/probe.py`, and a row in `boards/README.md`.
- **Say what you verified.** "Flashed, boot log clean, `artifacts/home.png`
  shows the clock at 12:00 with the new font" is done. "Should work" is not.

## Layout

```
firmware/            ESP-IDF 5.5 project (built with PlatformIO / pioarduino)
  platformio.ini     one env per board; PIXELLOOP_APP + PIXELLOOP_BOARD select what gets built
  src/apps/<app>/    probe (board identification), os (the PixelLoop OS)
  src/common/        code shared by every app
  sdkconfig.defaults chip defaults; sdkconfig.defaults.<board> for per-board overrides
boards/<id>/         board.h (pin map, display/touch parameters) + board-specific drivers
tools/               host side of the loop (Python, run with `uv run`, deps declared inline)
tests/               drive scripts + golden screenshots
artifacts/           per-run output: probe.json, boot.log, *.png, *.jpg (git-ignored)
```

## Device console protocol

The firmware speaks a line protocol on the USB console (115200 8N1, the baud
rate is irrelevant on USB-Serial/JTAG). Commands are plain text, replies are
prefixed so they can be filtered out of ordinary log output:

| command | reply |
|---|---|
| `shot` | `SHOT <w> <h> <fmt> <bytes>` then the raw framebuffer, then `SHOT-END` |
| `tap <x> <y>` | `OK` — synthesises a touch press/release at x,y |
| `swipe <x1> <y1> <x2> <y2> [ms]` | `OK` — synthesises a drag |
| `stats` | `STATS {json}` — free heap/PSRAM, uptime, active screen, input queue idle |
| `clock <seconds>` / `clock run` | `OK` — freeze the displayed time (test hook) / follow uptime again |
| `reset` | reboots |

Fixed console lines: `PIXELLOOP-READY` once the UI is up; the probe emits
`PROBE-BEGIN` / `PROBE-END` blocks.

## Toolchain facts

- PlatformIO with the pioarduino `espressif32` platform builds ESP-IDF 5.5 for
  the ESP32-S3. Use the core pioarduino installs at `~/.platformio/penv/bin/pio`
  (the Makefile does) or any PlatformIO core below 6.2 (CI pins
  `platformio>=6.1.18,<6.2`); core 6.2 pins SCons 4.11, the platform pins
  4.8.1, they reinstall each other mid-run and the link step fails with a
  missing `SCons.Tool` module.
  The very first ESP-IDF build on a machine installs
  tool packages (GDB, CMake, ninja) into `~/.platformio` and can take 10+
  minutes; later builds take seconds to a couple of minutes.
- `sdkconfig.defaults*` are only read when `firmware/sdkconfig.<env>` does not
  exist yet (standard ESP-IDF behaviour). After editing a defaults file, delete
  `firmware/sdkconfig.<env>` so the next build regenerates it.
- Host tools use `uv run <script>`; dependencies are declared in the script
  header (PEP 723), no virtualenv to manage.
- The USB-Serial/JTAG device enumerates as `/dev/ttyACM0` (`303a:1001`); the
  user must be in the `uucp`/`dialout` group.
