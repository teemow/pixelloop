# Tests

Drive scripts (`*.txt`) run unchanged against the device (`make test`, `make
drive SCRIPT=...`) and the desktop simulator (`make sim-test`, `make sim-drive
SCRIPT=...`). Golden screenshots live in `golden/<board>/` for the device and
`golden/sim/` for the simulator; `UPDATE=1` accepts new ones after you have
looked at them.
