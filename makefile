# ============================
# Makefile for OSAL Linux Demo
# ============================

# Compiler and flags
CC      := gcc
CFLAGS  := -Wall -O2 -pthread -Iinclude
LDFLAGS := -pthread

# Output
TARGET  := osal_demo

# Source and object files
SRC_DIR := src
OBJ_DIR := build

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	@echo "🔗 Linking $@ ..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compiling each .c
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "🧩 Compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if not exists
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean
clean:
	@echo "🧹 Cleaning build files..."
	rm -rf $(OBJ_DIR) $(TARGET)

# Run
run: $(TARGET)
	@echo "🚀 Running $(TARGET)..."
	./$(TARGET)

# Phony targets
.PHONY: all clean run
