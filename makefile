CC ?= $(CROSS_COMPILE)gcc
SRC_DIRS := hal/src unity/src
INC_DIRS := hal/include unity/include
OBJ_DIR  := out
# TEST_DIR  := src_unit_test/auto
# Test files
TEST_LOGIC_SRC := src_unit_test/auto/test_hal_gpio_linux_logic.c
TEST_HW_SRC    := src_unit_test/manual/test_hal_gpio_linux_hw.c

# Binary output
TEST_LOGIC_BIN := test_logic
TEST_HW_BIN    := test_hw

# libgpiod flags (ưu tiên pkg-config của SDK; nếu không có thì fallback -I/-L)
GPIOD_CFLAGS := $(shell pkg-config --cflags gpiod 2>/dev/null)
GPIOD_LIBS   := $(shell pkg-config --libs   gpiod 2>/dev/null)
ifeq ($(strip $(GPIOD_LIBS)),)
  ifneq ($(strip $(SDKTARGETSYSROOT)),)
    GPIOD_CFLAGS += -I$(SDKTARGETSYSROOT)/usr/include
    GPIOD_LIBS   += -L$(SDKTARGETSYSROOT)/usr/lib -lgpiod
  else
    GPIOD_LIBS   += -lgpiod
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
all: $(TEST_LOGIC_BIN)

# =========================
# Build logic test
# =========================
$(TEST_LOGIC_BIN): $(OBJS) $(TEST_LOGIC_SRC)
	@echo "🔧 Building $@ ..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# =========================
# Build HW test
# =========================
$(TEST_HW_BIN): $(OBJS) $(TEST_HW_SRC)
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

test-all: test-logic test-hw

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
	rm -rf $(OBJ_DIR) $(TEST_LOGIC_BIN) $(TEST_HW_BIN)
	rm -f *.gcno *.gcda *.info
	rm -rf coverage_html

.PHONY: all clean test-logic test-hw test-all coverage
