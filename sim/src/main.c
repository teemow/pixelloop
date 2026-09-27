// PixelLoop desktop simulator: the OS app's UI in an SDL2 window (or an
// offscreen surface), driven by the device console protocol on stdin/stdout.
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "board.h"
#include "cmds.h"
#include "console.h"
#include "lvgl.h"
#include "pixelloop.h"
#include "pl_port.h"
#include "pl_port_host.h"
#include "synth_input.h"
#include "ui.h"

static const char *TAG = "sim";

static void usage(FILE *to)
{
    fprintf(to,
            "pixelloop-sim [--headless] [--zoom N]\n"
            "  --headless   render offscreen (no window); automatic without a display\n"
            "  --zoom N     window scale factor (default 2)\n"
            "Console protocol on stdin/stdout, see AGENTS.md. Type `help`.\n");
}

static void lv_log_cb(lv_log_level_t level, const char *buf)
{
    static const char lvl[] = {'T', 'I', 'W', 'E', 'U'};
    char c = level >= 0 && (size_t)level < sizeof(lvl) ? lvl[level] : '?';
    // LVGL already formats a full line; keep it on the console stream.
    printf("%c lvgl: %s", c, buf);
    if (buf[0] == '\0' || buf[strlen(buf) - 1] != '\n') {
        printf("\n");
    }
    fflush(stdout);
}

// The panel has a fixed size. LVGL's SDL driver turns every window resize
// event into a display resolution change (window pixels / zoom), and under
// Wayland a stale resize event after zooming shrank the display to 120x140.
// Drop those events; the zoom below scales through SDL's logical size instead.
static int drop_window_resize(void *userdata, SDL_Event *e)
{
    return !(e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_RESIZED);
}

static void *console_thread(void *arg)
{
    console_run();  // returns on EOF: the host tool went away
    fflush(stdout);
    exit(0);
    return NULL;
}

int main(int argc, char **argv)
{
    bool headless = false;
    float zoom = 2.0f;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) {
            zoom = (float)atof(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        } else {
            usage(stderr);
            return 2;
        }
    }
    pl_host_init(argv);
    console_init();

    if (!headless && !getenv("SDL_VIDEODRIVER") && !getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
        headless = true;
    }
    if (headless && !getenv("SDL_VIDEODRIVER")) {
        setenv("SDL_VIDEODRIVER", "offscreen", 1);
    }
    PL_LOGI(TAG, "PixelLoop OS %s on %s (simulator%s)", PIXELLOOP_VERSION, PIXELLOOP_BOARD_NAME,
            headless ? ", headless" : "");

    lv_init();
    lv_log_register_print_cb(lv_log_cb);
    lv_display_t *disp = lv_sdl_window_create(BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    if (!disp) {
        PL_LOGE(TAG, "SDL window failed: %s", SDL_GetError());
        return 1;
    }
    lv_sdl_window_set_title(disp, "PixelLoop " PIXELLOOP_BOARD_NAME);
    SDL_SetEventFilter(drop_window_resize, NULL);
    if (!headless && zoom > 0 && zoom != 1.0f) {
        SDL_Window *win = lv_sdl_window_get_window(disp);
        SDL_Renderer *ren = lv_sdl_window_get_renderer(disp);
        SDL_SetWindowSize(win, (int)(BOARD_LCD_H_RES * zoom), (int)(BOARD_LCD_V_RES * zoom));
        SDL_RenderSetLogicalSize(ren, BOARD_LCD_H_RES, BOARD_LCD_V_RES);  // scales frames and mouse alike
    }
    lv_sdl_mouse_create();  // the human's finger
    PL_LOGI(TAG, "display %dx%d, video driver %s", (int)BOARD_LCD_H_RES, (int)BOARD_LCD_V_RES,
            SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");

    pl_lvgl_lock();
    synth_input_create(disp);
    ui_create();
    lv_refr_now(disp);  // first frame on screen before announcing readiness
    pl_lvgl_unlock();

    os_register_commands();
    printf("%s board=%s app=%s w=%d h=%d sim=1\n", PL_MARK_READY, PIXELLOOP_BOARD_NAME, PIXELLOOP_APP_NAME,
           (int)BOARD_LCD_H_RES, (int)BOARD_LCD_V_RES);
    fflush(stdout);

    pthread_t th;
    if (pthread_create(&th, NULL, console_thread, NULL) != 0) {
        PL_LOGE(TAG, "console thread failed");
        return 1;
    }
    pthread_detach(th);

    // Render loop, like the LVGL task on the device. SDL events (window,
    // mouse) are polled by the driver's own timer inside lv_timer_handler.
    for (;;) {
        pl_lvgl_lock();
        uint32_t wait = lv_timer_handler();
        pl_lvgl_unlock();
        SDL_Delay(wait < 5 ? wait : 5);
    }
}
