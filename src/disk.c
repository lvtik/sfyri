#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char last_disk_error[512] = "";

static void set_disk_error(const char *prefix, const char *detail) {
    if (detail && detail[0] != '\0') {
        snprintf(last_disk_error, sizeof(last_disk_error), "%s: %s", prefix, detail);
    } else {
        snprintf(last_disk_error, sizeof(last_disk_error), "%s", prefix);
    }
}

static void set_disk_errno(const char *prefix) {
    set_disk_error(prefix, strerror(errno));
}

const char *get_last_disk_error(void) {
    return last_disk_error[0] ? last_disk_error : "Unknown disk error";
}

static int append_disk(DiskList *list, size_t *capacity, const char *name, const char *path) {
    if (!name || !path || path[0] == '\0') return 0;

    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].path, path) == 0) return 0;
    }

    if (list->count == *capacity) {
        size_t next_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        Disk *next_items = realloc(list->items, next_capacity * sizeof(*next_items));
        if (!next_items) {
            set_disk_errno("Could not allocate disk list");
            return -1;
        }
        list->items = next_items;
        *capacity = next_capacity;
    }

    snprintf(list->items[list->count].name, sizeof(list->items[list->count].name), "%s", name);
    snprintf(list->items[list->count].path, sizeof(list->items[list->count].path), "%s", path);
    list->count++;
    return 0;
}

#if defined(__APPLE__) && defined(__MACH__)
static DiskList get_disk_list_macos(void) {
    DiskList list = {0};
    size_t capacity = 0;
    FILE *fp = popen("diskutil list", "r");
    if (!fp) {
        set_disk_errno("Could not run diskutil");
        return list;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char path[1024];
        if (sscanf(line, "%1023s", path) == 1 && strncmp(path, "/dev/disk", 9) == 0) {
            char *paren = strchr(path, '(');
            if (paren) *paren = '\0';

            const char *name = strrchr(path, '/');
            name = name ? name + 1 : path;
            if (append_disk(&list, &capacity, name, path) != 0) {
                break;
            }
        }
    }

    int result = pclose(fp);
    if (result != 0 && list.count == 0) {
        set_disk_error("diskutil list failed", NULL);
    } else if (list.count > 0) {
        last_disk_error[0] = '\0';
    }
    return list;
}
#else
static DiskList get_disk_list_linux(void) {
    DiskList list = {0};
    size_t capacity = 0;
    FILE *fp = popen("lsblk -d -n -o NAME,PATH,TYPE", "r");
    if (!fp) {
        set_disk_errno("Could not run lsblk");
        return list;
    }

    char name[256];
    char path[1024];
    char type[64];
    while (fscanf(fp, "%255s %1023s %63s", name, path, type) == 3) {
        if (strcmp(type, "disk") == 0 && append_disk(&list, &capacity, name, path) != 0) {
            break;
        }
    }

    int result = pclose(fp);
    if (result != 0 && list.count == 0) {
        set_disk_error("lsblk failed", NULL);
    } else if (list.count > 0) {
        last_disk_error[0] = '\0';
    }
    return list;
}
#endif

DiskList get_disk_list(void) {
#if defined(__APPLE__) && defined(__MACH__)
    return get_disk_list_macos();
#else
    return get_disk_list_linux();
#endif
}

void free_disk_list(DiskList *list) {
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
}
