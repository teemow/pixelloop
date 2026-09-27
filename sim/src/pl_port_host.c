// Host (POSIX) implementation of the port layer (pl_port.h) for the simulator.
#include "pl_port_host.h"

#include <malloc.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "pl_port.h"

static pthread_mutex_t s_lvgl_mutex;
static struct timespec s_t0;
static char **s_argv;

void pl_host_init(char **argv)
{
    s_argv = argv;
    clock_gettime(CLOCK_MONOTONIC, &s_t0);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s_lvgl_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

int64_t pl_uptime_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)(now.tv_sec - s_t0.tv_sec) * 1000000 + (now.tv_nsec - s_t0.tv_nsec) / 1000;
}

void pl_lvgl_lock(void)
{
    pthread_mutex_lock(&s_lvgl_mutex);
}

void pl_lvgl_unlock(void)
{
    pthread_mutex_unlock(&s_lvgl_mutex);
}

void *pl_alloc_large(size_t bytes)
{
    return malloc(bytes);
}

void pl_free_large(void *p)
{
    free(p);
}

// CRC-32 (IEEE 802.3 / zlib), bitwise; a screenshot is ~130 kB, fast enough.
uint32_t pl_crc32(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

size_t pl_base64_encode(char *dst, size_t dst_cap, const uint8_t *src, size_t len)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t need = (len + 2) / 3 * 4;
    if (dst_cap < need) {
        return 0;
    }
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)src[i] << 16;
        if (i + 1 < len) {
            v |= (uint32_t)src[i + 1] << 8;
        }
        if (i + 2 < len) {
            v |= src[i + 2];
        }
        dst[o++] = tbl[(v >> 18) & 63];
        dst[o++] = tbl[(v >> 12) & 63];
        dst[o++] = i + 1 < len ? tbl[(v >> 6) & 63] : '=';
        dst[o++] = i + 2 < len ? tbl[v & 63] : '=';
    }
    return o;
}

void pl_mem_stats(pl_mem_stats_t *out)
{
    struct mallinfo2 mi = mallinfo2();
    out->heap_free = mi.fordblks;
    out->heap_min = 0;
    out->psram_free = 0;
}

void pl_console_setup(void)
{
    // Replies must reach the pipe immediately, whether or not stdout is a tty.
    setvbuf(stdout, NULL, _IOLBF, 0);
}

void pl_restart(void)
{
    fflush(stdout);
    // Same binary, same arguments, same stdin/stdout pipes: the host tool
    // sees PIXELLOOP-READY again, like after the device's esp_restart().
    execv("/proc/self/exe", s_argv);
    perror("execv");
    _exit(1);
}

void pl_log(char level, const char *tag, const char *fmt, ...)
{
    // Same stream as the protocol replies, like the ESP log on the device;
    // host tools filter replies by prefix.
    printf("%c (%lld) %s: ", level, (long long)(pl_uptime_us() / 1000), tag);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}
