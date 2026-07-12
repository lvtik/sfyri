#include "ui.h"
#include "disk.h"
#include "img.h"
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IMAGE_PATH 1024
#define MAX_PICKER_PATH 1024

static DiskList disks = {0};
static char **disk_labels = NULL;
static int disk_scroll = 0;
static int selected_disk = -1;
static int focused_disk = -1;
static char image_path[MAX_IMAGE_PATH] = {0};
static bool image_path_edit = false;
static bool confirm_burn = false;
static bool show_file_picker = false;
static bool dark_mode = false;
static bool status_error = false;
static char status_text[512] = "Select an image and target disk.";
static FilePathList picker_files = {0};
static char **picker_labels = NULL;
static char picker_directory[MAX_PICKER_PATH] = {0};
static int picker_scroll = 0;
static int picker_selected = -1;
static int picker_focused = -1;

typedef struct {
    Color background;
    Color panel;
    Color text;
    Color line;
    Color base;
    Color focused;
    Color pressed;
    Color disabled;
    Color disabled_text;
    Color error;
    Color overlay;
} UiTheme;

static UiTheme current_theme = {0};

static void apply_theme(void) {
    if (dark_mode) {
        current_theme = (UiTheme){
            .background = {24, 27, 32, 255},
            .panel = {34, 39, 46, 255},
            .text = {232, 236, 241, 255},
            .line = {83, 91, 104, 255},
            .base = {45, 51, 60, 255},
            .focused = {63, 73, 86, 255},
            .pressed = {75, 92, 114, 255},
            .disabled = {42, 46, 52, 255},
            .disabled_text = {129, 138, 150, 255},
            .error = {232, 91, 91, 255},
            .overlay = {0, 0, 0, 120}
        };
    } else {
        current_theme = (UiTheme){
            .background = {245, 247, 250, 255},
            .panel = {255, 255, 255, 255},
            .text = {34, 40, 49, 255},
            .line = {191, 201, 214, 255},
            .base = {238, 242, 247, 255},
            .focused = {222, 231, 243, 255},
            .pressed = {198, 215, 236, 255},
            .disabled = {229, 233, 238, 255},
            .disabled_text = {142, 152, 164, 255},
            .error = {201, 52, 52, 255},
            .overlay = {0, 0, 0, 96}
        };
    }

    GuiLoadStyleDefault();
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(current_theme.line));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(current_theme.base));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(current_theme.text));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(current_theme.line));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(current_theme.focused));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(current_theme.text));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(current_theme.line));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(current_theme.pressed));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(current_theme.text));
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, ColorToInt(current_theme.line));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED, ColorToInt(current_theme.disabled));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED, ColorToInt(current_theme.disabled_text));
    GuiSetStyle(DEFAULT, LINE_COLOR, ColorToInt(current_theme.line));
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, ColorToInt(current_theme.panel));
}

static void set_status(bool is_error, const char *text) {
    status_error = is_error;
    snprintf(status_text, sizeof(status_text), "%s", text);
}

static void set_statusf(bool is_error, const char *format, const char *detail) {
    status_error = is_error;
    snprintf(status_text, sizeof(status_text), format, detail);
}

static void free_disk_labels(void) {
    if (!disk_labels) return;

    for (size_t i = 0; i < disks.count; i++) {
        free(disk_labels[i]);
    }
    free(disk_labels);
    disk_labels = NULL;
}

static void free_picker_entries(void) {
    if (picker_labels) {
        for (unsigned int i = 0; i < picker_files.count; i++) {
            free(picker_labels[i]);
        }
        free(picker_labels);
        picker_labels = NULL;
    }

    if (picker_files.paths) {
        UnloadDirectoryFiles(picker_files);
        picker_files = (FilePathList){0};
    }
}

static void load_picker_directory(const char *directory) {
    free_picker_entries();

    if (!directory || !DirectoryExists(directory)) {
        set_status(true, "Could not open that directory.");
        return;
    }

    snprintf(picker_directory, sizeof(picker_directory), "%s", directory);
    picker_files = LoadDirectoryFiles(picker_directory);
    picker_selected = -1;
    picker_focused = -1;
    picker_scroll = 0;

    if (picker_files.count == 0) return;

    picker_labels = calloc(picker_files.count, sizeof(*picker_labels));
    if (!picker_labels) {
        set_status(true, "Could not allocate file list.");
        free_picker_entries();
        return;
    }

    for (unsigned int i = 0; i < picker_files.count; i++) {
        bool is_file = IsPathFile(picker_files.paths[i]);
        const char *name = GetFileName(picker_files.paths[i]);
        const char *icon_label = GuiIconText(is_file ? ICON_FILE : ICON_FOLDER, name);

        picker_labels[i] = malloc(strlen(icon_label) + 1);
        if (!picker_labels[i]) {
            set_status(true, "Could not allocate file label.");
            free_picker_entries();
            return;
        }
        strcpy(picker_labels[i], icon_label);
    }
}

static void open_file_picker(void) {
    const char *directory = NULL;

    if (image_path[0] != '\0') {
        if (DirectoryExists(image_path)) {
            directory = image_path;
        } else {
            directory = GetDirectoryPath(image_path);
        }
    }

    if (!directory || directory[0] == '\0') {
        directory = GetWorkingDirectory();
    }

    load_picker_directory(directory);
    show_file_picker = true;
    image_path_edit = false;
}

static void choose_picker_selection(void) {
    if (picker_selected < 0 || (unsigned int)picker_selected >= picker_files.count) return;

    const char *selected_path = picker_files.paths[picker_selected];
    if (!IsPathFile(selected_path)) {
        load_picker_directory(selected_path);
        return;
    }

    long long size = get_image_size(selected_path);
    if (size < 0) {
        set_statusf(true, "%s", get_last_image_error());
        return;
    }

    snprintf(image_path, sizeof(image_path), "%s", selected_path);
    show_file_picker = false;
    set_status(false, "Image selected.");
}

static void refresh_disks(void) {
    free_disk_labels();
    free_disk_list(&disks);

    disks = get_disk_list();
    selected_disk = (disks.count > 0) ? 0 : -1;
    focused_disk = selected_disk;
    disk_scroll = 0;

    if (disks.count == 0) {
        const char *detail = get_last_disk_error();
        if (detail && strcmp(detail, "Unknown disk error") != 0) {
            set_statusf(true, "No disks found. %s", detail);
        } else {
            set_status(true, "No disks found.");
        }
        return;
    }

    disk_labels = calloc(disks.count, sizeof(*disk_labels));
    if (!disk_labels) {
        free_disk_list(&disks);
        selected_disk = -1;
        focused_disk = -1;
        set_status(true, "Could not allocate disk label list.");
        return;
    }

    for (size_t i = 0; i < disks.count; i++) {
        char label[1400];
        snprintf(label, sizeof(label), "%s    %s", disks.items[i].name, disks.items[i].path);
        disk_labels[i] = malloc(strlen(label) + 1);
        if (!disk_labels[i]) {
            set_status(true, "Could not allocate disk label.");
            free_disk_labels();
            free_disk_list(&disks);
            selected_disk = -1;
            focused_disk = -1;
            return;
        }
        strcpy(disk_labels[i], label);
    }

    set_status(false, "Disk list refreshed.");
}

static const char *format_size(long long size) {
    static char text[64];
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = (double)size;
    int unit = 0;

    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }

    snprintf(text, sizeof(text), "%.2f %s", value, units[unit]);
    return text;
}

static bool inputs_ready(void) {
    return image_path[0] != '\0' &&
        selected_disk >= 0 &&
        (size_t)selected_disk < disks.count &&
        get_image_size(image_path) >= 0;
}

static bool selected_inputs(BurnImage *image, const char **device) {
    if (image_path[0] == '\0') {
        set_status(true, "Enter an image path first.");
        return false;
    }

    if (selected_disk < 0 || (size_t)selected_disk >= disks.count) {
        set_status(true, "Select a target disk first.");
        return false;
    }

    long long size = get_image_size(image_path);
    if (size < 0) {
        set_statusf(true, "%s", get_last_image_error());
        return false;
    }

    image->size = size;
    snprintf(image->path, sizeof(image->path), "%s", image_path);
    *device = disks.items[selected_disk].path;
    return true;
}

void spawn_window() {
    InitWindow(900, 560, "sfyri");
    SetTargetFPS(60);
    apply_theme();
    refresh_disks();
}

void destroy_ui() {
    free_picker_entries();
    free_disk_labels();
    free_disk_list(&disks);
    CloseWindow();
}

void run(int buttonPressed, void (*action)(void)) {
    if (buttonPressed) {
        action();
    }
}

int spawn_message_box(const char *title, const char *message) {
    fprintf(stderr, "%s: %s\n", title, message);
    return 0;
}

void main_ui_loop() {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(current_theme.background);

        bool modal_open = confirm_burn || show_file_picker;
        if (modal_open) GuiLock();

        GuiPanel((Rectangle){24, 24, 852, 512}, NULL);
        GuiLabel((Rectangle){48, 44, 160, 24}, "sfyri");
        if (GuiButton((Rectangle){488, 40, 92, 28}, GuiIconText(ICON_LINK_NET, "Website"))) {
            OpenURL("https://lvtik.github.io");
        }
        if (GuiButton((Rectangle){592, 40, 92, 28}, GuiIconText(ICON_LINK, "GitHub"))) {
            OpenURL("https://github.com/lvtik/sfyri");
        }
        bool next_dark_mode = dark_mode;
        GuiToggle((Rectangle){696, 40, 156, 28}, dark_mode ? "Dark mode" : "Light mode", &next_dark_mode);
        if (next_dark_mode != dark_mode) {
            dark_mode = next_dark_mode;
            apply_theme();
        }
        GuiLine((Rectangle){48, 76, 804, 1}, NULL);

        GuiLabel((Rectangle){48, 100, 120, 24}, "Image path");
        if (GuiTextBox((Rectangle){48, 128, 548, 32}, image_path, MAX_IMAGE_PATH, image_path_edit)) {
            image_path_edit = !image_path_edit;
        }
        if (GuiButton((Rectangle){608, 128, 92, 32}, GuiIconText(ICON_FILE_OPEN, "Browse"))) {
            open_file_picker();
        }

        BurnImage image = {0};
        const char *device = NULL;
        long long image_size = (image_path[0] == '\0') ? -1 : get_image_size(image_path);
        bool valid_inputs = inputs_ready();
        if (image_size >= 0) {
            GuiLabel((Rectangle){716, 132, 136, 24}, format_size(image_size));
        } else {
            GuiLabel((Rectangle){716, 132, 136, 24}, "No image loaded");
        }

        GuiLabel((Rectangle){48, 180, 120, 24}, "Target disk");
        if (GuiButton((Rectangle){732, 176, 120, 28}, GuiIconText(ICON_RESTART, "Refresh"))) {
            refresh_disks();
        }

        if (disk_labels && disks.count > 0) {
            GuiListViewEx((Rectangle){48, 212, 804, 204}, disk_labels, (int)disks.count, &disk_scroll, &selected_disk, &focused_disk);
        } else {
            GuiPanel((Rectangle){48, 212, 804, 204}, NULL);
            GuiLabel((Rectangle){64, 230, 500, 24}, "No disks found.");
        }

        if (selected_disk >= 0 && (size_t)selected_disk < disks.count) {
            GuiLabel((Rectangle){48, 428, 804, 24}, TextFormat("Selected: %s", disks.items[selected_disk].path));
        } else {
            GuiLabel((Rectangle){48, 428, 804, 24}, "Selected: none");
        }

        GuiLabel((Rectangle){48, 464, 560, 24}, status_text);
        if (status_error) {
            DrawRectangleLinesEx((Rectangle){44, 456, 570, 40}, 1, current_theme.error);
        }

        if (!valid_inputs) GuiDisable();
        if (GuiButton((Rectangle){716, 460, 136, 36}, GuiIconText(ICON_FILE_EXPORT, "Burn"))) {
            confirm_burn = true;
        }
        if (!valid_inputs) GuiEnable();

        if (modal_open) GuiUnlock();

        if (confirm_burn) {
            int result = GuiMessageBox(
                (Rectangle){250, 180, 400, 180},
                "Confirm burn",
                "This writes the image to the selected disk.",
                "Cancel;Burn");

            if (result == 1) {
                confirm_burn = false;
            } else if (result == 2) {
                confirm_burn = false;
                if (selected_inputs(&image, &device) && burn(image, device) == 0) {
                    set_status(false, "Burn completed.");
                } else {
                    set_statusf(true, "Burn failed. %s", get_last_image_error());
                }
            }
        }

        if (show_file_picker) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), current_theme.overlay);

            Rectangle dialog = {170, 72, 560, 416};
            if (GuiWindowBox(dialog, "Select image")) {
                show_file_picker = false;
            }

            GuiLabel((Rectangle){194, 112, 430, 24}, picker_directory);
            if (GuiButton((Rectangle){638, 108, 68, 28}, GuiIconText(ICON_ARROW_UP, "Up"))) {
                const char *parent = GetPrevDirectoryPath(picker_directory);
                if (parent && parent[0] != '\0') load_picker_directory(parent);
            }

            if (picker_labels && picker_files.count > 0) {
                GuiListViewEx((Rectangle){194, 148, 512, 244}, picker_labels, (int)picker_files.count, &picker_scroll, &picker_selected, &picker_focused);
            } else {
                GuiPanel((Rectangle){194, 148, 512, 244}, NULL);
                GuiLabel((Rectangle){214, 170, 240, 24}, "No files in this folder.");
            }

            bool picker_can_select = picker_selected >= 0 && (unsigned int)picker_selected < picker_files.count;
            if (!picker_can_select) GuiDisable();
            if (GuiButton((Rectangle){476, 420, 110, 34}, GuiIconText(ICON_OK_TICK, "Select"))) {
                choose_picker_selection();
            }
            if (!picker_can_select) GuiEnable();

            if (GuiButton((Rectangle){596, 420, 110, 34}, "Cancel")) {
                show_file_picker = false;
            }
        }

        EndDrawing();
    }
}
