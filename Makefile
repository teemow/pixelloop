# PixelLoop — the agent test loop for ESP32 display firmware.
#
#   make probe                 identify the attached board (flashes the probe firmware)
#   make build BOARD=ws169     build the OS image for a board
#   make flash BOARD=ws169     build + flash
#   make run   BOARD=ws169     flash, then capture the boot log until PIXELLOOP-READY
#   make shot  NAME=home       screenshot the device framebuffer -> artifacts/home.png
#   make drive SCRIPT=tests/smoke.txt   send tap/swipe/shot commands from a script
#   make test  BOARD=ws169     run every test under tests/ against the device
#   make capture SECONDS=20    reset + record the raw console (any firmware)
#   make monitor               interactive serial console (Ctrl-C to leave)
#   make photo NAME=desk       webcam photo of the physical device -> artifacts/desk.jpg
#
# The same UI on the desktop, no device needed (sim/):
#   make sim                   build + open the simulator window (stdin is its console)
#   make sim-shot NAME=home    screenshot the simulator -> artifacts/sim/home.png
#   make sim-drive SCRIPT=tests/smoke.txt
#   make sim-test              every tests/*.txt against tests/golden/sim/
#   make sim-lvconf            refresh sim/lv_kconfig.h from firmware/sdkconfig.<board>
#
# BOARD defaults to the board identified by the last `make probe` (the
# simulator falls back to ws169). PORT defaults to the first Espressif
# USB-Serial/JTAG device.

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

# Desktop simulator (sim/): host CMake project, board geometry from boards/.
SIM_BOARD ?= $(or $(BOARD),ws169)
SIM_BUILD := sim/build
SIM_BIN   := $(SIM_BUILD)/pixelloop-sim
SIM_ART   := $(ART)/sim
CMAKE     ?= cmake

.PHONY: help probe build flash run shot drive test capture monitor photo clean need-board \
        sim sim-build sim-shot sim-drive sim-test sim-lvconf

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

capture: | $(ART) ## reset and record the console for SECONDS (default 20) -> artifacts/capture.log
	$(LOCK) $(UV) run tools/capture.py --port $(PORT) --seconds $(or $(SECONDS),20) --out $(ART)/capture.log

monitor: ## interactive serial console
	$(LOCK) bash -c 'cd $(FW) && $(PIO) device monitor -p $(PORT) -b 115200'

photo: | $(ART) ## webcam photo of the physical device -> artifacts/NAME.jpg
	tools/photo.sh $(ART)/$(NAME).jpg

# ---------------------------------------------------------------------------
# Desktop simulator. No port lock needed: every run is its own process.

sim-build: ## build the desktop simulator for SIM_BOARD (default: BOARD, else ws169)
	$(CMAKE) -S sim -B $(SIM_BUILD) -G Ninja -DPIXELLOOP_BOARD=$(SIM_BOARD) >/dev/null
	$(CMAKE) --build $(SIM_BUILD)

sim: sim-build ## run the simulator in a window; type console commands on stdin
	$(SIM_BIN)

sim-shot: sim-build | $(ART) ## screenshot the simulator -> artifacts/sim/NAME.png
	$(UV) run tools/shot.py --port sim:$(SIM_BIN) --out $(SIM_ART)/$(NAME).png

sim-drive: sim-build | $(ART) ## run a tap/swipe/shot script against the simulator
	@test -n "$(SCRIPT)" || { echo "pass SCRIPT=tests/<file>.txt"; exit 1; }
	$(UV) run tools/drive.py --port sim:$(SIM_BIN) --script $(SCRIPT) --out-dir $(SIM_ART)

sim-test: sim-build | $(ART) ## run every test under tests/ against the simulator (UPDATE=1 accepts goldens)
	@mkdir -p $(SIM_ART)
	$(UV) run tools/test.py --port sim:$(SIM_BIN) --board sim --out-dir $(SIM_ART) $(if $(UPDATE),--update,)

sim-lvconf: need-board ## regenerate sim/lv_kconfig.h from firmware/sdkconfig.BOARD (build the firmware first)
	@test -f $(FW)/sdkconfig.$(BOARD) || { echo "$(FW)/sdkconfig.$(BOARD) missing: run 'make build' first"; exit 1; }
	$(UV) run tools/lvconf.py --sdkconfig $(FW)/sdkconfig.$(BOARD) --out sim/lv_kconfig.h

clean: ## remove build output and artifacts
	rm -rf $(FW)/.pio $(SIM_BUILD) $(ART)
