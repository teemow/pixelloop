# PixelLoop — the agent test loop for ESP32 display firmware.
#
#   make probe                 identify the attached board (flashes the probe firmware)
#   make build BOARD=ws169     build the OS image for a board
#   make flash BOARD=ws169     build + flash
#   make run   BOARD=ws169     flash, then capture the boot log until PIXELLOOP-READY
#   make shot  NAME=home       screenshot the device framebuffer -> artifacts/home.png
#   make drive SCRIPT=tests/smoke.txt   send tap/swipe/shot commands from a script
#   make test  BOARD=ws169     run every test under tests/ against the device
#   make monitor               interactive serial console (Ctrl-C to leave)
#   make photo NAME=desk       webcam photo of the physical device -> artifacts/desk.jpg
#
# BOARD defaults to the board identified by the last `make probe`.
# PORT defaults to the first Espressif USB-Serial/JTAG device.

SHELL := /bin/bash
.DEFAULT_GOAL := help

# pioarduino ships its own PlatformIO core under ~/.platformio/penv whose
# SCons pin matches the platform; a distro `pio` (6.2, Python 3.14) reinstalls
# SCons on every run and the link step dies. Prefer the bundled core.
PIO   ?= $(shell test -x $$HOME/.platformio/penv/bin/pio && echo $$HOME/.platformio/penv/bin/pio || echo pio)
UV    ?= uv
FW    := firmware
ART   := artifacts
PORT  ?= $(shell tools/port.sh 2>/dev/null)
BOARD ?= $(shell test -f $(ART)/probe.json && python3 -c "import json;print(json.load(open('$(ART)/probe.json')).get('board') or '')" 2>/dev/null)
NAME  ?= shot
# The device has exactly one console. Every target that opens it takes this
# lock so a flash never races a monitor or a screenshot.
LOCK  := flock $(ART)/.port.lock

.PHONY: help probe build flash run shot drive test monitor photo clean need-board

help: ## list targets
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN{FS=":.*?## "}{printf "  %-10s %s\n", $$1, $$2}'

$(ART):
	@mkdir -p $(ART)

need-board:
	@test -n "$(BOARD)" || { echo "BOARD is empty: run 'make probe' or pass BOARD=<id> (see boards/README.md)"; exit 1; }

probe: | $(ART) ## flash the probe firmware and identify the attached board
	$(LOCK) bash -c 'cd $(FW) && $(PIO) run -e probe -t upload --upload-port $(PORT)'
	$(LOCK) $(UV) run tools/probe.py --port $(PORT) --out $(ART)/probe.json

build: need-board ## build the OS image for BOARD
	cd $(FW) && $(PIO) run -e $(BOARD)

flash: need-board | $(ART) ## build + flash BOARD
	$(LOCK) bash -c 'cd $(FW) && $(PIO) run -e $(BOARD) -t upload --upload-port $(PORT)'

run: flash ## flash, then capture the boot log until the READY marker
	$(LOCK) $(UV) run tools/boot.py --port $(PORT) --out $(ART)/boot.log

shot: | $(ART) ## screenshot the device framebuffer -> artifacts/NAME.png
	$(LOCK) $(UV) run tools/shot.py --port $(PORT) --out $(ART)/$(NAME).png

drive: | $(ART) ## run a tap/swipe/shot script against the device
	@test -n "$(SCRIPT)" || { echo "pass SCRIPT=tests/<file>.txt"; exit 1; }
	$(LOCK) $(UV) run tools/drive.py --port $(PORT) --script $(SCRIPT) --out-dir $(ART)

test: need-board | $(ART) ## run every test under tests/ against the device (UPDATE=1 accepts goldens)
	$(LOCK) $(UV) run tools/test.py --port $(PORT) --board $(BOARD) --out-dir $(ART) $(if $(UPDATE),--update,)

monitor: ## interactive serial console
	$(LOCK) bash -c 'cd $(FW) && $(PIO) device monitor -p $(PORT) -b 115200'

photo: | $(ART) ## webcam photo of the physical device -> artifacts/NAME.jpg
	tools/photo.sh $(ART)/$(NAME).jpg

clean: ## remove build output and artifacts
	rm -rf $(FW)/.pio $(ART)
