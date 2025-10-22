# ============================
# Makefile — OSAL Linux + libgpiod (cross via SDK)
# ============================

# CC đến từ SDK sau khi source environment-setup-*
CC ?= $(CROSS_COMPILE)gcc

# Dirs
SRC_DIR  := src
INC_DIR  := include
OBJ_DIR  := out
TARGET   := osal_demo

# libgpiod flags (ưu tiên pkg-config của SDK; nếu không có thì fallback -I/-L)
GPIOD_CFLAGS := $(shell pkg-config --cflags gpiod 2>/dev/null)
GPIOD_LIBS   := $(shell pkg-config --libs   gpiod 2>/dev/null)
ifeq ($(strip $(GPIOD_LIBS)),)
  # fallback: dùng sysroot từ SDK nếu có
  ifneq ($(strip $(SDKTARGETSYSROOT)),)
    GPIOD_CFLAGS += -I$(SDKTARGETSYSROOT)/usr/include
    GPIOD_LIBS   += -L$(SDKTARGETSYSROOT)/usr/lib -lgpiod
  else
    GPIOD_LIBS   += -lgpiod
  endif
endif

# Flags
CFLAGS  ?= -O2
CFLAGS  += -Wall -pthread -I$(INC_DIR) $(GPIOD_CFLAGS)
LDFLAGS ?=
LDFLAGS += -pthread $(GPIOD_LIBS)

# Debug (make DEBUG=1)
ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
endif

# Sources
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Default
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	@echo "🔗 Linking $@ ..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "🧩 Compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure out dir
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Utilities
clean:
	@echo "🧹 Cleaning ..."
	rm -rf $(OBJ_DIR) $(TARGET)

run: $(TARGET)
	@echo "🚀 Running $(TARGET) ..."
	./$(TARGET)

.PHONY: all clean run
