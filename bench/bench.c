/**
 * @file bench.c
 * @brief Bench build only: timing and stack measurements, faults injected from the UART.
 *
 * How it works
 * ------------
 * `make avr-bench` compiles this file with GG_BENCH; `make avr` never does
 * (FMEA F49). main.c calls it at four points: at the end of the boot, for each
 * byte read from the UART, after the tasks of a cycle, and after the watchdog
 * kick. It keeps the longest cycle and the longest display task since the boot,
 * and every 10 s prints `BENCH <s> <cycle ms> <display ms> <stack free>`.
 * Commands are `#` and a letter, so the command line never sees them. Each
 * injected fault only takes the detector to its safe side: `#o` and `#f`
 * stretch cycles past 100 ms (OVERRUN, then FAULT 3 after three in a row), and
 * `#h` stops the loop right after a kick, so the watchdog resets the MCU and the
 * relay drops. The PC times the gap between `BENCH HANG` and the next
 * `BENCH BOOT <ms>`, minus that boot time, to get the watchdog timeout.
 *
 * @req REQ-SAF-005 REQ-SAF-008 REQ-SAF-009
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bench/bench.h"
#include "common/gg_types/gg_types.h"
#include "hal/stack/stack.h"
#include "hal/timer/timer.h"
#include "hal/uart/uart.h"
#include "services/scheduler/scheduler.h"

/** @brief First byte of a bench command. */
#define BENCH_PREFIX 0x23u   /* '#': written as a number, cppcheck reads a quoted '#' as the # operator (MISRA 20.10). */
/** @brief Command letters: hang, one overrun, three overruns (FAULT), statistics. */
#define BENCH_CMD_HANG  ((uint8_t)'h')
#define BENCH_CMD_ONE   ((uint8_t)'o')
#define BENCH_CMD_FAULT ((uint8_t)'f')
#define BENCH_CMD_STATS ((uint8_t)'s')
/** @brief Line ends: they cancel a pending `#` and still reach the command line. */
#define BENCH_CR 0x0Du
#define BENCH_LF 0x0Au

/** @brief Length of a stretched cycle: 50 ms over the overrun limit, far below the 500 ms watchdog. */
#define BENCH_STALL_MS (SCHED_OVERRUN_MS + 50u)
/** @brief Period of the statistics line, in milliseconds. */
#define BENCH_REPORT_MS 10000u
/** @brief Milliseconds per second, for the uptime printed in the statistics. */
#define BENCH_MS_PER_S 1000u
/** @brief Longest text line: `BENCH` and four 10-digit numbers with spaces, CR, LF. */
#define BENCH_LINE_MAX 52u
/** @brief Digits of the largest uint32_t value. */
#define BENCH_DIGITS_MAX 10u
/** @brief Base of the printed numbers. */
#define BENCH_BASE 10u

/** @brief State of the bench build: one instance for the whole program. */
typedef struct {
    bool prefix;              /**< The last byte was `#`. */
    bool hang;                /**< `#h` received: hang after the next kick. */
    bool report_now;          /**< `#s` received: print the statistics at the next cycle end. */
    uint8_t stalls;           /**< Cycles still to stretch. */
    uint32_t max_cycle_ms;    /**< Longest cycle since the boot. */
    uint32_t max_display_ms;  /**< Longest display task since the boot. */
    uint32_t last_report_ms;  /**< Time of the last statistics line. */
} bench_t;

static bench_t s_bench;

/** @brief A text line being built, with its length. */
typedef struct {
    uint8_t text[BENCH_LINE_MAX];  /**< Bytes of the line. */
    uint8_t len;                   /**< Bytes used. */
} bench_line_t;

/** @brief The line being built: one at a time, the program has a single thread. */
static bench_line_t s_line;

/**
 * @brief Append a zero-terminated text to a line; what does not fit is dropped.
 * @param line  Line to extend.
 * @param text  Text to append.
 */
static void line_text(bench_line_t *line, const char *text)
{
    uint8_t i = 0u;

    while ((text[i] != '\0') && (line->len < BENCH_LINE_MAX)) {
        line->text[line->len] = (uint8_t)text[i];
        line->len++;
        i++;
    }
}

/**
 * @brief Append a space and a decimal number to a line.
 * @param line   Line to extend.
 * @param value  Number to print.
 */
static void line_number(bench_line_t *line, uint32_t value)
{
    static const char k_digits[BENCH_BASE + 1u] = "0123456789";   /* Indexed by value. */
    char digits[BENCH_DIGITS_MAX + 2u];   /* Space, up to 10 digits, terminator. */
    uint8_t pos = BENCH_DIGITS_MAX + 1u;
    uint32_t rest = value;

    digits[pos] = '\0';
    do {
        pos--;
        digits[pos] = k_digits[rest % BENCH_BASE];
        rest /= BENCH_BASE;
    } while ((rest > 0u) && (pos > 1u));
    pos--;
    digits[pos] = ' ';
    line_text(line, &digits[pos]);
}

/**
 * @brief Send a line, ended with CR LF.
 * @param line  Line to send.
 * @return The status of the UART: GG_ERR_TIMEOUT when the transmit buffer is full.
 */
static gg_status_t line_send(bench_line_t *line)
{
    line_text(line, "\r\n");
    return hal_uart_write(line->text, line->len);
}

/**
 * @brief Print the statistics line: uptime, longest cycle, longest display task, free stack.
 * @param now_ms  Current time.
 * @return The first error, or GG_OK.
 */
static gg_status_t report(uint32_t now_ms)
{
    uint16_t free_bytes = 0u;
    gg_status_t status = hal_stack_free_bytes(&free_bytes);

    if (status == GG_OK) {
        s_line.len = 0u;
        line_text(&s_line, "BENCH");
        line_number(&s_line, now_ms / BENCH_MS_PER_S);
        line_number(&s_line, s_bench.max_cycle_ms);
        line_number(&s_line, s_bench.max_display_ms);
        line_number(&s_line, (uint32_t)free_bytes);
        status = line_send(&s_line);
    }
    return status;
}

/**
 * @brief Stop the loop: the watchdog is no longer kicked and resets the MCU.
 *
 * Interrupts stay enabled, so the `BENCH HANG` line still leaves the UART.
 */
static void hang(void)
{
    s_line.len = 0u;
    line_text(&s_line, "BENCH HANG");
    (void)line_send(&s_line);   /* The hang happens even if the line is lost. */
    for (;;) {
        /* Wait for the watchdog reset (REQ-SAF-005). */
    }
}

/**
 * @brief Run the command of one letter received after `#`. An unknown letter does nothing.
 * @param letter  Byte received after `#`.
 */
static void run_letter(uint8_t letter)
{
    if (letter == BENCH_CMD_HANG) {
        s_bench.hang = true;
    } else if (letter == BENCH_CMD_ONE) {
        s_bench.stalls = 1u;
    } else if (letter == BENCH_CMD_FAULT) {
        s_bench.stalls = (uint8_t)SCHED_OVERRUN_FAULT_COUNT;
    } else if (letter == BENCH_CMD_STATS) {
        s_bench.report_now = true;
    } else {
        /* Unknown letter: dropped, nothing injected. */
    }
}

gg_status_t bench_boot(void)
{
    uint32_t now_ms = 0u;
    gg_status_t status = hal_timer_get_ms(&now_ms);

    s_bench.prefix = false;
    s_bench.hang = false;
    s_bench.report_now = false;
    s_bench.stalls = 0u;
    s_bench.max_cycle_ms = 0u;
    s_bench.max_display_ms = 0u;
    s_bench.last_report_ms = now_ms;
    if (status == GG_OK) {
        s_line.len = 0u;
        line_text(&s_line, "BENCH BOOT");
        line_number(&s_line, now_ms);
        status = line_send(&s_line);
    }
    return status;
}

gg_status_t bench_take_byte(uint8_t byte, bool *taken)
{
    gg_status_t status = GG_OK;

    if (taken == NULL) {
        status = GG_ERR_NULL;
    } else if (!s_bench.prefix) {
        s_bench.prefix = (byte == BENCH_PREFIX);
        *taken = s_bench.prefix;
    } else if ((byte == BENCH_CR) || (byte == BENCH_LF)) {
        s_bench.prefix = false;   /* `#` alone on a line: dropped, the line end goes on. */
        *taken = false;
    } else if (byte == BENCH_PREFIX) {
        *taken = true;            /* `##`: still waiting for the letter. */
    } else {
        s_bench.prefix = false;
        *taken = true;
        run_letter(byte);
    }
    return status;
}

gg_status_t bench_stall(void)
{
    gg_status_t status = GG_OK;

    if (s_bench.stalls > 0u) {
        uint32_t start_ms = 0u;
        uint32_t now_ms = 0u;

        s_bench.stalls--;
        status = hal_timer_get_ms(&start_ms);
        now_ms = start_ms;
        while ((status == GG_OK) && ((now_ms - start_ms) < BENCH_STALL_MS)) {
            status = hal_timer_get_ms(&now_ms);
        }
    }
    return status;
}

gg_status_t bench_display_time(uint32_t duration_ms)
{
    if (duration_ms > s_bench.max_display_ms) {
        s_bench.max_display_ms = duration_ms;
    }
    return GG_OK;
}

gg_status_t bench_after_cycle(uint32_t start_ms, uint32_t end_ms)
{
    gg_status_t status = GG_OK;
    uint32_t duration_ms = end_ms - start_ms;

    if (duration_ms > s_bench.max_cycle_ms) {
        s_bench.max_cycle_ms = duration_ms;
    }
    if (s_bench.hang) {
        hang();
    } else if (s_bench.report_now || ((end_ms - s_bench.last_report_ms) >= BENCH_REPORT_MS)) {
        s_bench.report_now = false;
        s_bench.last_report_ms = end_ms;
        /* A full transmit buffer only loses this line: the next one comes 10 s later. */
        status = report(end_ms);
        if (status == GG_ERR_TIMEOUT) {
            status = GG_OK;
        }
    } else {
        /* Nothing to do in this cycle. */
    }
    return status;
}
