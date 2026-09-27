# PixelLoop

An operating system for ESP32 displays, built by coding agents, with the
**hardware-in-the-loop test loop** that makes agent development possible:

```
change code  →  flash the device  →  drive the UI  →  screenshot  →  iterate
```

Everything an agent needs is a `make` target, everything it sees is a file
under `artifacts/`. The first target hardware is the Waveshare
ESP32-S3-Touch-LCD family (round and rectangular, 1.28" to 7"), but the board
layer is the only part that knows about a specific panel.

## Status

Bootstrapping. What exists today:

- `make probe` — identifies the attached board (chip, flash, PSRAM, I2C
  fingerprint) and records it for the other targets.
- The firmware project (ESP-IDF 5.5 via PlatformIO), the board reference for
  seven Waveshare boards, and the agent playbook in [AGENTS.md](AGENTS.md).

Next: the OS app (LVGL 9), the console protocol for screenshots and synthetic
input, the desktop simulator, and golden-image tests.

## Quick start

```sh
git clone https://github.com/teemow/pixelloop && cd pixelloop
make probe                  # plug in the board first; writes artifacts/probe.json
make run                    # flash the OS for the identified board, capture boot log
make shot NAME=home         # artifacts/home.png
make drive SCRIPT=tests/smoke.txt
```

Requirements: PlatformIO (`pip install platformio` or your distro package),
`uv`, and a user that may open `/dev/ttyACM*`. Optional: `ffmpeg` for webcam
photos of the physical board.

## Why a loop and not a simulator only

A desktop simulator gives fast, deterministic screenshots, and PixelLoop will
have one. But displays are hardware: SPI timings, PSRAM bandwidth, touch
controller quirks, backlight PWM and panel orientation only show up on the real
thing. The loop makes the real thing as cheap to poke as the simulator.

## Layout

See [AGENTS.md](AGENTS.md#layout). Board pinouts live in
[boards/README.md](boards/README.md).

## License

[MIT](LICENSE)
