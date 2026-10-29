/**
 * @file hal_wdt_fake.h
 * @brief Host-only control API of the watchdog fake (PC tests only, never on AVR).
 */
#ifndef HAL_WDT_FAKE_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_WDT_FAKE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Disable the fake watchdog and clear its kick counter.
 * @req REQ-SAF-005
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
void hal_wdt_fake_reset(void);

/**
 * @brief Report whether hal_wdt_enable() has been called.
 * @return true once the watchdog is enabled.
 * @req REQ-SAF-005
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
bool hal_wdt_fake_is_enabled(void);

/**
 * @brief Number of times hal_wdt_kick() was called since the reset.
 * @req REQ-SAF-005
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
uint16_t hal_wdt_fake_kicks(void);

#endif /* HAL_WDT_FAKE_H */
