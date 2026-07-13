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

#if (defined(__APPLE__) && defined(__MACH__)) || defined(_WIN32)
static void rtrim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}
#endif

static int append_disk(DiskList *list, size_t *capacity, const char *name, const char *path,
    const char *label, bool is_internal) {
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

    Disk *disk = &list->items[list->count];
    snprintf(disk->name, sizeof(disk->name), "%s", name);
    snprintf(disk->path, sizeof(disk->path), "%s", path);
    snprintf(disk->label, sizeof(disk->label), "%s", label ? label : "");
    disk->is_internal = is_internal;
    list->count++;
    return 0;
}

#if defined(__APPLE__) && defined(__MACH__)
/* diskutil list only prints a bare device path, so this looks up the
   human-readable media name and internal/external flag per disk from
   `diskutil info`. Treats lookup failure as internal (the safer default
   for a tool whose whole job is erasing whatever disk you point it at). */
static void fill_disk_details_macos(const char *path, char *label, size_t label_size, bool *is_internal) {
    label[0] = '\0';
    *is_internal = true;

    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "diskutil info \"%s\" 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *value = strstr(line, "Media Name:");
        if (value) {
            value += strlen("Media Name:");
            while (*value == ' ' || *value == '\t') value++;
            rtrim(value);
            if (label[0] == '\0') snprintf(label, label_size, "%s", value);
            continue;
        }

        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (strncmp(trimmed, "Device Location:", 16) == 0) {
            char *v = trimmed + 16;
            while (*v == ' ' || *v == '\t') v++;
            *is_internal = (strncmp(v, "Internal", 8) == 0);
        }
    }

    pclose(fp);
}

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

            char label[256];
            bool is_internal;
            fill_disk_details_macos(path, label, sizeof(label), &is_internal);

            if (append_disk(&list, &capacity, name, path, label, is_internal) != 0) {
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
#elif defined(_WIN32)
static DiskList get_disk_list_windows(void) {
    DiskList list = {0};
    size_t capacity = 0;
    FILE *fp = _popen(
        "powershell -NoProfile -Command "
        "\"Get-CimInstance Win32_DiskDrive | ForEach-Object "
        "{ '{0}|{1}|{2}' -f $_.DeviceID,$_.Caption,$_.InterfaceType }\"",
        "r");
    if (!fp) {
        set_disk_errno("Could not run PowerShell");
        return list;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        rtrim(line);

        char *path = line;
        char *sep1 = strchr(path, '|');
        if (!sep1) continue;
        *sep1 = '\0';

        char *caption = sep1 + 1;
        char *sep2 = strchr(caption, '|');
        if (!sep2) continue;
        *sep2 = '\0';

        const char *interface_type = sep2 + 1;
        const char *name = (caption[0] != '\0') ? caption : path;

        /* USB is the closest signal Win32_DiskDrive gives us for "this is
           a removable stick, not the disk Windows booted from". */
        bool is_internal = (strcmp(interface_type, "USB") != 0);

        if (strncmp(path, "\\\\.\\PHYSICALDRIVE", 18) != 0) continue;
        if (append_disk(&list, &capacity, name, path, caption, is_internal) != 0) break;
    }

    int result = _pclose(fp);
    if (result != 0 && list.count == 0) {
        set_disk_error("PowerShell disk query failed", NULL);
    } else if (list.count > 0) {
        last_disk_error[0] = '\0';
    }
    return list;
}
#else
static bool extract_kv(const char *line, const char *key, char *out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", key);

    const char *start = strstr(line, pattern);
    if (!start) return false;
    start += strlen(pattern);

    const char *end = strchr(start, '"');
    if (!end) return false;

    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static DiskList get_disk_list_linux(void) {
    DiskList list = {0};
    size_t capacity = 0;
    FILE *fp = popen("lsblk -d -n -P -o NAME,PATH,TYPE,RM,MODEL", "r");
    if (!fp) {
        set_disk_errno("Could not run lsblk");
        return list;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char type[64];
        if (!extract_kv(line, "TYPE", type, sizeof(type)) || strcmp(type, "disk") != 0) continue;

        char name[256] = {0};
        char path[1024] = {0};
        char removable[8] = {0};
        char model[256] = {0};
        extract_kv(line, "NAME", name, sizeof(name));
        extract_kv(line, "PATH", path, sizeof(path));
        extract_kv(line, "RM", removable, sizeof(removable));
        extract_kv(line, "MODEL", model, sizeof(model));

        /* lsblk's "removable" flag is the standard heuristic for USB/SD
           media; anything not flagged removable is treated as internal. */
        bool is_internal = (removable[0] != '1');

        if (append_disk(&list, &capacity, name, path, model, is_internal) != 0) break;
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
#elif defined(_WIN32)
    return get_disk_list_windows();
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
