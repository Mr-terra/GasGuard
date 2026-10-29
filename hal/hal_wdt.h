/**
 * @file hal_wdt.h
 * @brief Hardware interface: hardware watchdog timer.
 *
 * Once enabled it cannot be disabled by software: a stalled main loop always
 * ends in a reset (REQ-SAF-005, failure mode F07).
 *
 * @req REQ-SAF-005
 */
#ifndef HAL_WDT_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_WDT_H

#include "common/gg_types.h"

/**
 * @brief Enable the watchdog with a 500 ms timeout.
 * @return GG_OK on success.
 * @req REQ-SAF-005
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_wdt_enable(void);

/**
 * @brief Restart the watchdog countdown. Called once per main-loop pass,
 *        and only after every periodic task has returned.
 * @return GG_OK on success.
 * @req REQ-SAF-005
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_wdt_kick(void);

#endif /* HAL_WDT_H */
