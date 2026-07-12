#ifndef IMG_H
#define IMG_H
typedef struct {
    long long size;
    char path[1024];
} BurnImage;
long long get_image_size(const char *path);
int burn(BurnImage img, const char *device);
const char *get_last_image_error(void);
#endif
