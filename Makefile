# GasGuard build system (GNU make).
# Run inside WSL2 (Ubuntu) from the repository root: `make check`.
# Targets are described in CLAUDE.md ("Commands").

CC        ?= gcc
AVR_CC    ?= avr-gcc
PYTHON    ?= python3

BUILD     := build
SRC_DIR   := src
TEST_DIR  := tests
UNITY_DIR := third_party/unity

# Compiler flags from docs/04-coding-standard.md section 1.
STD_FLAGS  := -std=c99
WARN_FLAGS := -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion \
              -Wshadow -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes \
              -Wundef -Wswitch-enum
INCLUDES   := -I$(SRC_DIR) -I$(UNITY_DIR)

HOST_CFLAGS := $(STD_FLAGS) $(WARN_FLAGS) $(INCLUDES) -O0 -g \
               --coverage -fsanitize=address,undefined

# Host code: everything under src/ except the AVR drivers and the firmware entry point.
LIB_SRCS   := $(shell find $(SRC_DIR) -name '*.c' -not -path '$(SRC_DIR)/hal/avr/*' -not -name 'main.c' 2>/dev/null)
UNITY_SRCS := $(UNITY_DIR)/unity.c
TEST_SRCS  := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS  := $(patsubst $(TEST_DIR)/%.c,$(BUILD)/test/%,$(TEST_SRCS))

.PHONY: all test check clean

all: test

# Each test file is linked with the host library code and Unity, then run.
test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t"; ./$$t || exit 1; done

$(BUILD)/test/%: $(TEST_DIR)/%.c $(LIB_SRCS) $(UNITY_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) $^ -o $@

check: test

clean:
	rm -rf $(BUILD)
