#include "img.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

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

    int file = open(path, O_RDONLY);
    if (file < 0) {
        set_image_errno("Could not open image");
        return -1;
    }

    off_t size = lseek(file, 0, SEEK_END);
    if (size < 0) {
        set_image_errno("Could not read image size");
        close(file);
        return -1;
    }

    close(file);
    last_image_error[0] = '\0';
    return (long long)size;
}

int burn(BurnImage img, const char *device) {
    if (img.path[0] == '\0') {
        set_image_error("Image path is empty", NULL);
        return -1;
    }
    if (!device || device[0] == '\0') {
        set_image_error("Target device is empty", NULL);
        return -1;
    }

    printf("Burning image: %s (size: %lld) to device: %s\n", img.path, img.size, device);
    size_t buff_size = (img.size > 1024LL * 1024LL * 1024LL) ? 4U * 1024U * 1024U : 1024U * 1024U;
    int src = open(img.path, O_RDONLY);
    if (src < 0) {
        set_image_errno("Could not open image");
        return -1;
    }
    int dest = open(device, O_WRONLY);
    if (dest < 0) {
        set_image_errno("Could not open target device");
        close(src);
        return -1;
    }
    char *buffer = malloc(buff_size);
    if (!buffer) {
        set_image_errno("Could not allocate burn buffer");
        close(src);
        close(dest);
        return -1;
    }
    ssize_t bytesRead;
    while ((bytesRead = read(src, buffer, buff_size)) > 0) {
        ssize_t totalWritten = 0;
        while (totalWritten < bytesRead) {
            ssize_t bytesWritten = write(dest, buffer + totalWritten, (size_t)(bytesRead - totalWritten));
            if (bytesWritten < 0) {
                if (errno == EINTR) continue;
                set_image_errno("Could not write to target device");
                free(buffer);
                close(src);
                close(dest);
                return -1;
            }
            if (bytesWritten == 0) {
                set_image_error("Could not write to target device", "wrote 0 bytes");
                free(buffer);
                close(src);
                close(dest);
                return -1;
            }
            totalWritten += bytesWritten;
        }
    }
    if (bytesRead < 0) {
        set_image_errno("Could not read image");
        free(buffer);
        close(src);
        close(dest);
        return -1;
    }
    if (fsync(dest) != 0) {
        set_image_errno("Could not sync target device");
        free(buffer);
        close(src);
        close(dest);
        return -1;
    }
    free(buffer);
    close(src);
    close(dest);
    last_image_error[0] = '\0';
    return 0;
}
