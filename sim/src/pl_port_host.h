#pragma once
// Host-only additions to the port layer (pl_port.h).
// Remember argv (for `reset`, which re-execs the process) and start the clock.
void pl_host_init(char **argv);
