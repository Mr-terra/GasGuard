/**
 * @file bench.h
 * @brief Bench build only (`make avr-bench`, GG_BENCH): timing and stack
 *        measurements, and faults injected on purpose from the UART.
 *
 * Never part of `make avr`: main.c calls these functions only inside
 * `#ifdef GG_BENCH` blocks (FMEA F49).
 *
 * @req REQ-SAF-005 REQ-SAF-008 REQ-SAF-009
 */
#ifndef BENCH_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define BENCH_H

#include <stdbool.h>
#include <stdint.h>

#include "common/gg_types/gg_types.h"

/**
 * @brief Print the bench banner with the boot time: `BENCH BOOT <ms>`.
 *
 * The PC subtracts this time from the gap between `BENCH HANG` and the banner
 * to get the watchdog timeout.
 * @return GG_OK, or the error of the timer or the UART.
 * @req REQ-SAF-005
 */
// DEV-05: bench API, called by main.c (outside the lint scope) and its test; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t bench_boot(void);

/**
 * @brief Take the bytes of a bench command (`#` then one letter) out of the UART stream.
 *
 * `#h` hangs the loop after the next watchdog kick, `#o` makes the next cycle
 * last 150 ms, `#f` makes the next three cycles last 150 ms, `#s` prints the
 * statistics now. Any other byte is left to the command line.
 * @param byte   Byte read from the UART.
 * @param taken  Receives true when the byte belongs to a bench command. Must not be NULL.
 * @return GG_OK or GG_ERR_NULL.
 * @req REQ-SAF-005 REQ-SAF-008
 */
// DEV-05: bench API, called by main.c (outside the lint scope) and its test; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t bench_take_byte(uint8_t byte, bool *taken);

/**
 * @brief Make the current cycle longer than the overrun limit, if a command asked for it.
 *
 * Called after the tasks and before the end time of the cycle is read.
 * @return GG_OK, or the error of the timer.
 * @req REQ-SAF-008
 */
// DEV-05: bench API, called by main.c (outside the lint scope) and its test; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t bench_stall(void);

/**
 * @brief Record the duration of the display task.
 * @param duration_ms  Time spent in the display task, in milliseconds.
 * @return GG_OK.
 * @req REQ-SAF-008
 */
// DEV-05: bench API, called by main.c (outside the lint scope) and its test; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t bench_display_time(uint32_t duration_ms);

/**
 * @brief End of a cycle, after the watchdog kick: record its duration, print
 *        the statistics every 10 s, and hang if `#h` was received.
 * @param start_ms  Time at the start of the cycle.
 * @param end_ms    Time after the last task.
 * @return GG_OK, or the error of the UART or the stack driver. Does not return after `#h`.
 * @req REQ-SAF-005 REQ-SAF-008 REQ-SAF-009
 */
// DEV-05: bench API, called by main.c (outside the lint scope) and its test; cppcheck sees one caller
// cppcheck-suppress misra-c2012-8.7
gg_status_t bench_after_cycle(uint32_t start_ms, uint32_t end_ms);

#endif /* BENCH_H */
