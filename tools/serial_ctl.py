# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pillow>=10", "numpy>=1.26"]
# ///
"""Shared device access for the PixelLoop host tools.

Every tool talks to the board over the ESP32-S3's native USB-Serial/JTAG port
using the line protocol documented in AGENTS.md. This module owns port
discovery, the reset dance, log capture and the screenshot decoder so each
tool stays a few lines long.

The same protocol is spoken by the desktop simulator on its stdin/stdout.
`--port sim:` launches `sim/build/pixelloop-sim` (`sim:<path>` for another
binary) and talks to it over a pipe; every tool works unchanged on either.
"""
from __future__ import annotations

import base64
import glob
import os
import select
import subprocess
import sys
import time
import zlib
from typing import IO

READY = "PIXELLOOP-READY"
CRASH_MARKERS = ("Guru Meditation", "abort()", "assert failed", "rst:0x7 (TG0WDT", "rst:0x8 (TG1WDT",
                 "Task watchdog got triggered", "Stack smashing", "Brownout")
SIM_PREFIX = "sim:"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM_DEFAULT = os.path.join(REPO, "sim", "build", "pixelloop-sim")


def find_port(explicit: str | None = None) -> str:
    """Explicit argument, then $PIXELLOOP_PORT, then the first Espressif
    USB-Serial/JTAG device, then /dev/ttyACM0. `sim:` selects the simulator."""
    if explicit:
        return explicit
    env = os.environ.get("PIXELLOOP_PORT")
    if env:
        return env
    by_id = sorted(glob.glob("/dev/serial/by-id/*Espressif*"))
    if by_id:
        return os.path.realpath(by_id[0])
    acm = sorted(glob.glob("/dev/ttyACM*"))
    if acm:
        return acm[0]
    sys.exit("no serial port found; pass --port (or --port sim: for the simulator) or set PIXELLOOP_PORT")


def rgb565_to_png(raw: bytes, w: int, h: int, path: str) -> None:
    import numpy as np
    from PIL import Image

    px = np.frombuffer(raw, dtype="<u2").reshape(h, w)
    r = ((px >> 11) & 0x1F).astype(np.uint16)
    g = ((px >> 5) & 0x3F).astype(np.uint16)
    b = (px & 0x1F).astype(np.uint16)
    rgb = np.stack(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)), axis=-1).astype(np.uint8)
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    Image.fromarray(rgb, "RGB").save(path)


def rle_decode(buf: bytes, expected: int) -> bytes:
    out = bytearray(expected)
    i = o = 0
    n = len(buf)
    while i + 2 < n and o < expected:
        run = buf[i]
        px = buf[i + 1:i + 3]
        end = min(o + run * 2, expected)
        out[o:end] = px * ((end - o) // 2)
        o = end
        i += 3
    if o != expected:
        raise ValueError(f"rle: decoded {o} of {expected} bytes")
    return bytes(out)


class SerialTransport:
    """The board's USB-Serial/JTAG console."""

    def __init__(self, port: str, baud: int):
        import serial  # type: ignore

        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = 0.1
        # Do not assert DTR/RTS on open: USB-Serial/JTAG maps them to
        # GPIO0/EN and would drop the chip into the bootloader.
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()

    def read(self, timeout: float) -> bytes:
        """Whatever has arrived, waiting at most `timeout` for the first byte."""
        self.ser.timeout = timeout
        first = self.ser.read(1)
        if not first:
            return b""
        return first + self.ser.read(self.ser.in_waiting)

    def write(self, data: bytes) -> None:
        self.ser.write(data)
        self.ser.flush()

    def reset(self) -> None:
        """Hard reset into the application (RTS pulse, DTR low = GPIO0 high)."""
        self.ser.dtr = False
        self.ser.rts = True
        time.sleep(0.1)
        self.ser.rts = False
        time.sleep(0.05)
        self.ser.reset_input_buffer()

    def close(self) -> None:
        self.ser.close()


class PipeTransport:
    """The simulator as a child process; its stdin/stdout are the console."""

    def __init__(self, binary: str):
        if not os.path.isfile(binary) or not os.access(binary, os.X_OK):
            sys.exit(f"simulator not built: {binary} (run `make sim-build`)")
        self.argv = [binary, "--headless"]
        self.proc: subprocess.Popen | None = None
        self._start()

    def _start(self) -> None:
        self.proc = subprocess.Popen(self.argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)

    def read(self, timeout: float) -> bytes:
        assert self.proc and self.proc.stdout
        fd = self.proc.stdout.fileno()
        ready, _, _ = select.select([fd], [], [], timeout)
        if not ready:
            return b""
        data = os.read(fd, 65536)
        if not data:
            raise RuntimeError(f"simulator exited with code {self.proc.wait()}")
        return data

    def write(self, data: bytes) -> None:
        assert self.proc and self.proc.stdin
        try:
            self.proc.stdin.write(data)
            self.proc.stdin.flush()
        except BrokenPipeError as e:
            raise RuntimeError(f"simulator exited with code {self.proc.wait()}") from e

    def reset(self) -> None:
        """A fresh process: the closest thing to a power cycle."""
        self.close()
        self._start()

    def close(self) -> None:
        if not self.proc:
            return
        proc, self.proc = self.proc, None
        try:
            if proc.stdin:
                proc.stdin.close()  # EOF on stdin makes the simulator exit
            proc.wait(timeout=2.0)
        except (subprocess.TimeoutExpired, OSError):
            proc.kill()
            proc.wait()
        if proc.stdout:
            proc.stdout.close()


def open_transport(port: str, baud: int) -> SerialTransport | PipeTransport:
    if port.startswith(SIM_PREFIX):
        return PipeTransport(port[len(SIM_PREFIX):] or SIM_DEFAULT)
    return SerialTransport(port, baud)


class Device:
    """One open console. Tees everything read to `log` when given."""

    def __init__(self, port: str | None = None, baud: int = 115200, log: IO[str] | None = None, echo: bool = False):
        self.port = find_port(port)
        self.log = log
        self.echo = echo
        self.io = open_transport(self.port, baud)
        self._buf = b""

    def close(self) -> None:
        self.io.close()

    # -- low level -----------------------------------------------------------
    def reset(self) -> None:
        """Restart the firmware: hard reset on the device, a new process for the simulator."""
        self.io.reset()
        self._buf = b""

    def readline(self, timeout: float) -> str | None:
        deadline = time.monotonic() + timeout
        while True:
            if b"\n" in self._buf:
                raw, self._buf = self._buf.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                self._tee(line)
                return line
            left = deadline - time.monotonic()
            if left <= 0:
                return None
            self._buf += self.io.read(min(left, 0.1))

    def _tee(self, line: str) -> None:
        if self.log:
            self.log.write(line + "\n")
            self.log.flush()
        if self.echo:
            print(line, flush=True)

    def send(self, line: str) -> None:
        self.io.write((line + "\n").encode())

    # -- protocol ------------------------------------------------------------
    def wait_for(self, marker: str, timeout: float) -> list[str]:
        """Collect lines until one contains `marker`. Raises TimeoutError."""
        lines: list[str] = []
        deadline = time.monotonic() + timeout
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError(f"'{marker}' not seen within {timeout}s")
            line = self.readline(min(left, 1.0))
            if line is None:
                continue
            lines.append(line)
            if marker in line:
                return lines

    def cmd(self, line: str, timeout: float = 5.0) -> list[str]:
        """Send a command, return the lines up to and including OK/ERR."""
        self.send(line)
        lines: list[str] = []
        deadline = time.monotonic() + timeout
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError(f"no OK/ERR for '{line}' within {timeout}s")
            got = self.readline(min(left, 1.0))
            if got is None:
                continue
            lines.append(got)
            if got.startswith("OK") or got.startswith("ERR"):
                if got.startswith("ERR"):
                    raise RuntimeError(f"{line!r}: {got}")
                return lines

    def stats(self) -> dict:
        import json
        for line in self.cmd("stats"):
            if line.startswith("STATS "):
                return json.loads(line[6:])
        raise RuntimeError("no STATS line")

    def shot(self, path: str, timeout: float = 20.0) -> tuple[int, int]:
        """Ask for the framebuffer and write it as PNG. Returns (w, h)."""
        self.send("shot")
        header = None
        deadline = time.monotonic() + timeout
        while header is None:
            line = self.readline(max(0.1, deadline - time.monotonic()))
            if line is None:
                raise TimeoutError("no SHOT header")
            if line.startswith("SHOT "):
                header = line.split()
            elif line.startswith("ERR"):
                raise RuntimeError(line)
        _, w, h, fmt, raw_len, enc_len, crc_hex = header
        w, h, raw_len, enc_len = int(w), int(h), int(raw_len), int(enc_len)
        chunks: list[str] = []
        while True:
            line = self.readline(max(0.1, deadline - time.monotonic()))
            if line is None:
                raise TimeoutError("screenshot transfer stalled")
            if line.startswith("SHOT-END"):
                break
            chunks.append(line.strip())
        enc = base64.b64decode("".join(chunks), validate=False)
        if len(enc) != enc_len:
            raise ValueError(f"screenshot: got {len(enc)} encoded bytes, expected {enc_len}")
        raw = rle_decode(enc, raw_len) if fmt == "rgb565rle" else enc
        if len(raw) != raw_len:
            raise ValueError(f"screenshot: got {len(raw)} raw bytes, expected {raw_len}")
        crc = zlib.crc32(raw) & 0xFFFFFFFF
        if crc != int(crc_hex, 16):
            raise ValueError(f"screenshot: crc {crc:08x} != {crc_hex}")
        rgb565_to_png(raw, w, h, path)
        # swallow the trailing OK
        self.wait_for("OK", 2.0)
        return w, h

    def tap(self, x: int, y: int) -> None:
        self.cmd(f"tap {x} {y}")

    def swipe(self, x1: int, y1: int, x2: int, y2: int, ms: int = 250) -> None:
        self.cmd(f"swipe {x1} {y1} {x2} {y2} {ms}")


def scan_crashes(lines: list[str]) -> list[str]:
    return [l for l in lines if any(m in l for m in CRASH_MARKERS)]
