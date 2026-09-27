#include "console.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pl_port.h"

#define MAX_CMDS 24
#define MAX_ARGS 12

typedef struct {
    const char *name;
    console_cmd_fn fn;
    const char *help;
} cmd_t;

static cmd_t s_cmds[MAX_CMDS];
static int s_ncmds;

void console_ok(void)
{
    printf("OK\n");
    fflush(stdout);
}

void console_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("ERR ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    fflush(stdout);
}

static void cmd_help(int argc, char **argv)
{
    for (int i = 0; i < s_ncmds; i++) {
        printf("  %-8s %s\n", s_cmds[i].name, s_cmds[i].help ? s_cmds[i].help : "");
    }
    console_ok();
}

static void cmd_reset(int argc, char **argv)
{
    console_ok();
    pl_restart();
}

void console_register(const char *name, console_cmd_fn fn, const char *help)
{
    if (s_ncmds < MAX_CMDS) {
        s_cmds[s_ncmds++] = (cmd_t){name, fn, help};
    }
}

void console_init(void)
{
    pl_console_setup();
    console_register("help", cmd_help, "list commands");
    console_register("reset", cmd_reset, "reboot");
}

void console_run(void)
{
    char line[256];
    char *argv[MAX_ARGS];
    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strcspn(line, "\r\n");
        line[n] = '\0';
        int argc = 0;
        char *save = NULL;
        for (char *tok = strtok_r(line, " \t", &save); tok && argc < MAX_ARGS;
             tok = strtok_r(NULL, " \t", &save)) {
            argv[argc++] = tok;
        }
        if (argc == 0) {
            continue;
        }
        const cmd_t *hit = NULL;
        for (int i = 0; i < s_ncmds; i++) {
            if (strcmp(s_cmds[i].name, argv[0]) == 0) {
                hit = &s_cmds[i];
                break;
            }
        }
        if (hit) {
            hit->fn(argc, argv);
        } else {
            console_err("unknown command '%s' (try help)", argv[0]);
        }
    }
}
