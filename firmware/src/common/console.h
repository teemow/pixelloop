#pragma once
// Line-oriented command console on the platform's console stream (the
// USB-Serial/JTAG port on the device, stdin/stdout in the simulator).
// Replies start with OK / ERR so the host can tell them from log output.
#include <stdbool.h>

typedef void (*console_cmd_fn)(int argc, char **argv);

void console_init(void);
void console_register(const char *name, console_cmd_fn fn, const char *help);
// Blocks forever reading lines and dispatching commands. Run from app_main.
void console_run(void);
// Helpers for command implementations.
void console_ok(void);
void console_err(const char *fmt, ...);
