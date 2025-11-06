CC ?= $(CROSS_COMPILE)gcc
SRC_DIRS := hal/src unity/src osal/src
INC_DIRS := hal/include unity/include osal/include
OBJ_DIR  := out
# TEST_DIR  := src_unit_test/auto
# Test files
TEST_LOGIC_SRC := src_unit_test/auto/test_hal_gpio_linux_logic.c
TEST_HW_SRC    := src_unit_test/manual/test_hal_gpio_linux_hw.c
TEST_UART_SRC  := src_unit_test/manual/test_hal_uart_linux.c
TEST_I2C_SRC   := src_unit_test/manual/test_hal_i2c_linux.c
TEST_SPI_SRC   := src_unit_test/manual/test_hal_spi_linux.c

TEST_OSAL_SRC  := src_unit_test/osal/test_osal_task_linux.c

# Binary output
TEST_LOGIC_BIN := test_logic
TEST_HW_BIN    := test_hw
TEST_UART_BIN  := test_uart
TEST_I2C_BIN   := test_i2c
TEST_SPI_BIN   := test_spi
TEST_OSAL_BIN  := test_osal

# libgpiod flags (ưu tiên pkg-config của SDK; nếu không có thì fallback -I/-L)
GPIOD_CFLAGS := $(shell pkg-config --cflags gpiod 2>/dev/null)
GPIOD_LIBS   := $(shell pkg-config --libs   gpiod 2>/dev/null)
ifeq ($(strip $(GPIOD_LIBS)),)
  ifneq ($(strip $(SDKTARGETSYSROOT)),)
    GPIOD_CFLAGS += -I$(SDKTARGETSYSROOT)/usr/include
    GPIOD_LIBS   += -L$(SDKTARGETSYSROOT)/usr/lib -lgpiod
    GPIOD_LIBS   += -lutil
  else
    GPIOD_LIBS   += -lgpiod
    GPIOD_LIBS   += -lutil
  endif
endif

# Flags
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CFLAGS  ?= -O2
CFLAGS  += -Wall -pthread $(INC_FLAGS) $(GPIOD_CFLAGS)
CFLAGS  += -DUNITY_INCLUDE_VERBOSE
LDFLAGS ?=
LDFLAGS += -pthread $(GPIOD_LIBS)

# Nếu build để đo coverage: make COVERAGE=1
ifeq ($(COVERAGE),1)
  CFLAGS  += --coverage
  LDFLAGS += --coverage
endif

# Debug (make DEBUG=1)
ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
endif

# Sources & Objects
SRCS := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))

# =========================
# Default
# =========================
all: $(TEST_LOGIC_BIN) $(TEST_HW_BIN) $(TEST_OSAL_BIN) $(TEST_UART_BIN) $(TEST_I2C_BIN) $(TEST_SPI_BIN)

# =========================
# Build logic test
# =========================
$(TEST_LOGIC_BIN): $(OBJS) $(TEST_LOGIC_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Build UART test
# =========================
$(TEST_UART_BIN): $(OBJS) $(TEST_UART_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Build I2C test
# =========================
$(TEST_I2C_BIN): $(OBJS) $(TEST_I2C_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Build I2C test
# =========================
$(TEST_SPI_BIN): $(OBJS) $(TEST_SPI_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Build HW test
# =========================
$(TEST_HW_BIN): $(OBJS) $(TEST_HW_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Build OSAL test
# =========================
$(TEST_OSAL_BIN): $(OBJS) $(TEST_OSAL_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Compile .c -> out/.../.o
# =========================
$(OBJ_DIR)/%.o: %.c
	@echo "🧩 Compiling $< ..."
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# =========================
# Run tests
# =========================
test-logic: $(TEST_LOGIC_BIN)
	@echo "🚀 Running logic test..."
	./$(TEST_LOGIC_BIN)

test-hw: $(TEST_HW_BIN)
	@echo "🚀 Running HW test..."
	./$(TEST_HW_BIN) || true

test-uart: $(TEST_UART_BIN)
	@echo "🚀 Running UART test..."
	./$(TEST_UART_BIN) || true

test-i2c: $(TEST_I2C_BIN)
	@echo "🚀 Running I2C test..."
	./$(TEST_I2C_BIN) || true

test-spi: $(TEST_SPI_BIN)
	@echo "🚀 Running SPI test..."
	./$(TEST_SPI_BIN) || true

test-osal: $(TEST_OSAL_BIN)
	@echo "🚀 Running OSAL test..."
	./$(TEST_OSAL_BIN) || true

test-all: test-logic test-hw test-osal test-i2c test-spi

# =========================
# Coverage
# =========================
coverage: test-all
	@echo "📊 Generating coverage report ..."
	lcov --capture --directory . --output-file coverage.info
	lcov --remove coverage.info '/usr/*' --output-file coverage.info
	genhtml coverage.info --output-directory coverage_html
	@echo "➡  Open coverage_html/index.html"

# =========================
# Clean
# =========================
clean:
	@echo "🧹 Cleaning ..."
	rm -rf $(OBJ_DIR) $(TEST_LOGIC_BIN) $(TEST_HW_BIN) $(TEST_OSAL_BIN) $(TEST_UART_BIN) $(TEST_I2C_BIN) $(TEST_SPI_BIN)
	rm -f *.gcno *.gcda *.info
	rm -rf coverage_html

.PHONY: all clean test-logic test-hw test-all coverage
