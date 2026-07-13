#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char name[256];
    char path[1024];
    char label[256];
    bool is_internal;
} Disk;

typedef struct {
    Disk *items;
    size_t count;
} DiskList;

DiskList get_disk_list(void);
void free_disk_list(DiskList *list);
const char *get_last_disk_error(void);

#endif
