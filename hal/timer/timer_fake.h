/**
 * @file timer_fake.h
 * @brief Host-only control API of the timer fake (PC tests only, never on AVR).
 */
#ifndef HAL_TIMER_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_TIMER_FAKE_H

#include <stdint.h>

/**
 * @brief Set the uptime to 0 ms.
 * @req REQ-LOG-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_timer_fake_reset(void);

/**
 * @brief Script the uptime that the next hal_timer_get_ms() reports.
 * @param ms  Uptime in milliseconds.
 * @req REQ-LOG-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_timer_fake_set_ms(uint32_t ms);

/**
 * @brief Make the uptime move forward by itself after each hal_timer_get_ms().
 *
 * For code that waits in a loop on the clock (the bench stall). 0, the value
 * after hal_timer_fake_reset(), keeps the clock still.
 * @param step_ms  Milliseconds added after each read.
 * @req REQ-LOG-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_timer_fake_set_step(uint32_t step_ms);

#endif /* HAL_TIMER_FAKE_H */
