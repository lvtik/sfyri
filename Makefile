.PHONY: all clean rebuild

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -std=c99
LDFLAGS := -lm

# Directories
INCLUDE_DIR := include
SRC_DIR := src
LIB_RAYGUI_DIR := lib/raygui
BUILD_DIR := build
BIN_DIR := bin

# Include paths
CFLAGS += -I$(INCLUDE_DIR) -isystem $(LIB_RAYGUI_DIR) -DPLATFORM_DESKTOP

UNAME_S := $(shell uname -s 2>/dev/null)

# Windows_NT is set by the OS itself (works under cmd.exe, PowerShell and
# MSYS2/MinGW shells alike); the MINGW*/MSYS* uname fallback covers shells
# that don't inherit $(OS).
ifeq ($(OS),Windows_NT)
	IS_WINDOWS := 1
else ifneq (,$(findstring MINGW,$(UNAME_S)))
	IS_WINDOWS := 1
else ifneq (,$(findstring MSYS,$(UNAME_S)))
	IS_WINDOWS := 1
else
	IS_WINDOWS :=
endif

ifeq ($(IS_WINDOWS),1)
	TARGET := $(BIN_DIR)/sfyri.exe
	RAYLIB_CFLAGS ?=
	RAYLIB_LDFLAGS ?= -lraylib
	CFLAGS += $(RAYLIB_CFLAGS)
	LDFLAGS += $(RAYLIB_LDFLAGS) -lopengl32 -lgdi32 -lwinmm
else
	TARGET := $(BIN_DIR)/sfyri
	CFLAGS += -D_POSIX_C_SOURCE=200809L
	RAYLIB_CFLAGS ?= -isystem /opt/homebrew/include -isystem /usr/local/include
	RAYLIB_LDFLAGS ?= -L/opt/homebrew/lib -L/usr/local/lib -lraylib
	CFLAGS += $(RAYLIB_CFLAGS)
	LDFLAGS += $(RAYLIB_LDFLAGS)

	ifeq ($(UNAME_S),Darwin)
		LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	else
		LDFLAGS += -lGL -ldl -lpthread -lrt -lX11
	endif
endif

# Source files
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Default target
all: $(TARGET)

# Create directories
$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@

# Compile main source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Rebuild target
rebuild: clean all
