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
CFLAGS += -I$(INCLUDE_DIR) -isystem $(LIB_RAYGUI_DIR) -DPLATFORM_DESKTOP -D_POSIX_C_SOURCE=200809L
RAYLIB_CFLAGS ?= -isystem /opt/homebrew/include -isystem /usr/local/include
RAYLIB_LDFLAGS ?= -L/opt/homebrew/lib -L/usr/local/lib -lraylib
CFLAGS += $(RAYLIB_CFLAGS)
LDFLAGS += $(RAYLIB_LDFLAGS)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
	LDFLAGS += -lGL -ldl -lpthread -lrt -lX11
endif

# Source files
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Output executable
TARGET := $(BIN_DIR)/sfyri

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
