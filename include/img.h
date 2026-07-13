#ifndef IMG_H
#define IMG_H

#include <stdbool.h>

typedef struct {
    long long size;
    char path[1024];
} BurnImage;

typedef enum {
    BURN_IDLE = 0,
    BURN_RUNNING,
    BURN_DONE,
    BURN_FAILED
} BurnState;

long long get_image_size(const char *path);
const char *get_last_image_error(void);

bool burn_start(BurnImage img, const char *device);
BurnState burn_get_progress(long long *written, long long *total);
void burn_reset(void);

#endif
