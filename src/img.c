#include "img.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#endif

static char last_image_error[512] = "";

static void set_image_error(const char *prefix, const char *detail) {
    if (detail && detail[0] != '\0') {
        snprintf(last_image_error, sizeof(last_image_error), "%s: %s", prefix, detail);
    } else {
        snprintf(last_image_error, sizeof(last_image_error), "%s", prefix);
    }
}

static void set_image_errno(const char *prefix) {
    set_image_error(prefix, strerror(errno));
}

const char *get_last_image_error(void) {
    return last_image_error[0] ? last_image_error : "Unknown image error";
}

long long get_image_size(const char *path) {
    if (!path || path[0] == '\0') {
        set_image_error("Image path is empty", NULL);
        return -1;
    }

#ifdef _WIN32
    struct __stat64 st;
    if (_stat64(path, &st) != 0) {
        set_image_errno("Could not open image");
        return -1;
    }
    if (!(st.st_mode & _S_IFREG)) {
        set_image_error("Not a regular file", NULL);
        return -1;
    }
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        set_image_errno("Could not open image");
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        set_image_error("Not a regular file", NULL);
        return -1;
    }
#endif

    last_image_error[0] = '\0';
    return (long long)st.st_size;
}

/* ---- Async burn: writes on a background thread so the UI stays responsive ---- */

static BurnImage g_burn_image;
static char g_burn_device[1024];
static volatile long long g_burn_written = 0;
static volatile long long g_burn_total = 0;
static volatile BurnState g_burn_state = BURN_IDLE;
static bool g_burn_thread_valid = false;

#ifdef _WIN32
static HANDLE g_burn_thread;
#else
static pthread_t g_burn_thread;
#endif

static bool do_burn(BurnImage img, const char *device, volatile long long *written) {
    if (img.path[0] == '\0') {
        set_image_error("Image path is empty", NULL);
        return false;
    }
    if (!device || device[0] == '\0') {
        set_image_error("Target device is empty", NULL);
        return false;
    }

    FILE *src = fopen(img.path, "rb");
    if (!src) {
        set_image_errno("Could not open image");
        return false;
    }

    size_t buff_size = (img.size > 1024LL * 1024LL * 1024LL) ? (4U * 1024U * 1024U) : (1024U * 1024U);
    char *buffer = malloc(buff_size);
    if (!buffer) {
        set_image_errno("Could not allocate burn buffer");
        fclose(src);
        return false;
    }

#ifdef _WIN32
    HANDLE dest = CreateFileA(device, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
    if (dest == INVALID_HANDLE_VALUE) {
        set_image_error("Could not open target device", NULL);
        free(buffer);
        fclose(src);
        return false;
    }
#else
    int dest = open(device, O_WRONLY);
    if (dest < 0) {
        set_image_errno("Could not open target device");
        free(buffer);
        fclose(src);
        return false;
    }
#endif

    *written = 0;
    bool ok = true;

    for (;;) {
        size_t bytesRead = fread(buffer, 1, buff_size, src);
        if (bytesRead == 0) {
            if (ferror(src)) {
                set_image_error("Could not read image", NULL);
                ok = false;
            }
            break;
        }

        size_t totalWritten = 0;
        while (totalWritten < bytesRead) {
#ifdef _WIN32
            DWORD bytesWritten = 0;
            if (!WriteFile(dest, buffer + totalWritten, (DWORD)(bytesRead - totalWritten), &bytesWritten, NULL)) {
                set_image_error("Could not write to target device", NULL);
                ok = false;
                break;
            }
#else
            ssize_t bytesWritten = write(dest, buffer + totalWritten, bytesRead - totalWritten);
            if (bytesWritten < 0) {
                if (errno == EINTR) continue;
                set_image_errno("Could not write to target device");
                ok = false;
                break;
            }
            if (bytesWritten == 0) {
                set_image_error("Could not write to target device", "wrote 0 bytes");
                ok = false;
                break;
            }
#endif
            totalWritten += (size_t)bytesWritten;
            *written += (long long)bytesWritten;
        }
        if (!ok) break;
    }

#ifdef _WIN32
    if (ok && !FlushFileBuffers(dest)) {
        set_image_error("Could not sync target device", NULL);
        ok = false;
    }
    CloseHandle(dest);
#else
    if (ok && fsync(dest) != 0) {
        set_image_errno("Could not sync target device");
        ok = false;
    }
    close(dest);
#endif

    free(buffer);
    fclose(src);
    if (ok) last_image_error[0] = '\0';
    return ok;
}

#ifdef _WIN32
static DWORD WINAPI burn_thread_main(LPVOID arg) {
    (void)arg;
    bool ok = do_burn(g_burn_image, g_burn_device, &g_burn_written);
    g_burn_state = ok ? BURN_DONE : BURN_FAILED;
    return 0;
}
#else
static void *burn_thread_main(void *arg) {
    (void)arg;
    bool ok = do_burn(g_burn_image, g_burn_device, &g_burn_written);
    g_burn_state = ok ? BURN_DONE : BURN_FAILED;
    return NULL;
}
#endif

bool burn_start(BurnImage img, const char *device) {
    if (g_burn_state == BURN_RUNNING) {
        set_image_error("A burn is already in progress", NULL);
        return false;
    }
    burn_reset();

    g_burn_image = img;
    snprintf(g_burn_device, sizeof(g_burn_device), "%s", device ? device : "");
    g_burn_written = 0;
    g_burn_total = img.size;
    g_burn_state = BURN_RUNNING;

#ifdef _WIN32
    g_burn_thread = CreateThread(NULL, 0, burn_thread_main, NULL, 0, NULL);
    g_burn_thread_valid = (g_burn_thread != NULL);
#else
    g_burn_thread_valid = (pthread_create(&g_burn_thread, NULL, burn_thread_main, NULL) == 0);
#endif

    if (!g_burn_thread_valid) {
        set_image_error("Could not start burn thread", NULL);
        g_burn_state = BURN_FAILED;
        return false;
    }
    return true;
}

BurnState burn_get_progress(long long *written, long long *total) {
    if (written) *written = g_burn_written;
    if (total) *total = g_burn_total;
    return g_burn_state;
}

void burn_reset(void) {
    if (g_burn_thread_valid) {
#ifdef _WIN32
        WaitForSingleObject(g_burn_thread, INFINITE);
        CloseHandle(g_burn_thread);
#else
        pthread_join(g_burn_thread, NULL);
#endif
        g_burn_thread_valid = false;
    }
    if (g_burn_state != BURN_RUNNING) g_burn_state = BURN_IDLE;
}
