# GasGuard build system (GNU make).
# Run inside WSL2 (Ubuntu) from the repository root: `make check`.
# Targets are described in CLAUDE.md ("Commands").

CC        ?= gcc
AVR_CC    ?= avr-gcc
PYTHON    ?= python3

BUILD     := build
TEST_DIR  := tests
UNITY_DIR := third_party/unity

# Layer folders at the repository root (docs/02-architecture.md §1).
# main.c sits at the root too; it is the only file outside these folders.
MODULE_DIRS := common hal app services drivers

# Compiler flags from docs/04-coding-standard.md section 1.
STD_FLAGS  := -std=c99
WARN_FLAGS := -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion \
              -Wshadow -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes \
              -Wundef -Wswitch-enum
INCLUDES   := -I. -I$(UNITY_DIR)

HOST_CFLAGS := $(STD_FLAGS) $(WARN_FLAGS) $(INCLUDES) -O0 -g \
               --coverage -fsanitize=address,undefined

# Host code: the layer folders minus the AVR drivers (hal/avr is never built on the PC).
LIB_SRCS   := $(shell find $(MODULE_DIRS) -name '*.c' -not -path 'hal/avr/*' 2>/dev/null)
UNITY_SRCS := $(UNITY_DIR)/unity.c
TEST_SRCS  := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS  := $(patsubst $(TEST_DIR)/%.c,$(BUILD)/test/%,$(TEST_SRCS))

# AVR firmware: main.c plus the layer folders minus the host fakes.
AVR_MCU    := atmega328p
AVR_FLAGS  := -mmcu=$(AVR_MCU) -DF_CPU=16000000UL $(STD_FLAGS) $(WARN_FLAGS) \
              $(INCLUDES) -Os -ffunction-sections -fdata-sections -Wl,--gc-sections
AVR_SRCS   := main.c $(shell find $(MODULE_DIRS) -name '*.c' -not -path 'hal/host/*' 2>/dev/null)
AVR_ELF    := $(BUILD)/avr/gasguard.elf
AVR_HEX    := $(BUILD)/avr/gasguard.hex

# Memory budget (REQ-NFR-005): flash <= 80 % of 32 KB, RAM <= 60 % of 2 KB.
FLASH_BUDGET := 26214
RAM_BUDGET   := 1228

# Public headers, each checked alone for self-containment.
HEADERS      := $(shell find $(MODULE_DIRS) -name '*.h' 2>/dev/null)
COVERAGE_DIR := $(BUILD)/coverage

.PHONY: all test check lint coverage avr size trace clean

all: test

# Each test file is linked with the host library code and Unity, then run.
test: $(TEST_BINS)
	@rm -f $(BUILD)/test/*.gcda # stale coverage counters from older runs
	@for t in $(TEST_BINS); do echo "== $$t"; ./$$t || exit 1; done

$(BUILD)/test/%: $(TEST_DIR)/%.c $(LIB_SRCS) $(UNITY_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(HOST_CFLAGS) $^ -o $@

# Static analysis gates (docs/04-coding-standard.md §6). Any finding fails the build.
lint:
	cppcheck --enable=all --addon=misra --error-exitcode=1 --std=c99 \
	         --suppress=missingIncludeSystem --inline-suppr -I. $(MODULE_DIRS) $(TEST_DIR)
	clang-tidy --warnings-as-errors='*' $(LIB_SRCS) -- $(STD_FLAGS) $(INCLUDES)
	lizard -C 10 -L 60 -w $(MODULE_DIRS)
	@if grep -rn '<avr/' app services 2>/dev/null; then \
	    echo "R20: app/ and services/ must not include AVR headers"; exit 1; fi
	@for h in $(HEADERS); do \
	    printf '#include "%s"\n' "$$h" | $(CC) $(HOST_CFLAGS) -fsyntax-only -x c - || exit 1; done

# Line coverage report (REQ-NFR-004). The 90 % gate is enabled on Day 2.
# @req REQ-NFR-004
# @verifies REQ-NFR-004
coverage: test
	@mkdir -p $(COVERAGE_DIR)
	gcovr -r . --object-directory $(BUILD)/test \
	      --filter '(common|hal|app|services|drivers)/' \
	      --html-details $(COVERAGE_DIR)/index.html --print-summary

# Cross-compile the firmware for the ATmega328P.
$(AVR_ELF): $(AVR_SRCS)
	@mkdir -p $(dir $@)
	$(AVR_CC) $(AVR_FLAGS) $^ -o $@

$(AVR_HEX): $(AVR_ELF)
	avr-objcopy -O ihex -R .eeprom $< $@

avr: $(AVR_HEX)

# Memory budget check on the ELF file (REQ-NFR-005).
# @req REQ-NFR-005
# @verifies REQ-NFR-005
size: $(AVR_ELF)
	avr-size --mcu=$(AVR_MCU) -C $(AVR_ELF)
	avr-size --format=berkeley $(AVR_ELF) | \
	    awk -v flash_max=$(FLASH_BUDGET) -v ram_max=$(RAM_BUDGET) -f tools/size_check.awk

# Requirement traceability. --allow-gaps until Day 5 of PLAN.md.
TRACE_FLAGS ?= --allow-gaps
trace:
	$(PYTHON) tools/trace.py $(TRACE_FLAGS)

check: test lint coverage avr size trace

clean:
	rm -rf $(BUILD)
