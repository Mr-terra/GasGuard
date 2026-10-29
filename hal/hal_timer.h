/**
 * @file hal_timer.h
 * @brief Hardware interface: 1 ms system tick and uptime counter.
 *
 * The tick drives the cooperative scheduler (docs/02-architecture.md §4) and
 * the uptime stamps of the event log (REQ-LOG-002).
 *
 * @req REQ-FUN-001 REQ-LOG-002
 */
#ifndef HAL_TIMER_H
// DEV-02: include guard, not an unused macro
// cppcheck-suppress misra-c2012-2.5
#define HAL_TIMER_H

#include <stdint.h>

#include "common/gg_types.h"

/**
 * @brief Start the 1 ms tick.
 * @return GG_OK on success, GG_ERR_HW if the timer cannot be configured.
 * @req REQ-FUN-001
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_timer_init(void);

/**
 * @brief Read the milliseconds elapsed since hal_timer_init().
 *
 * Implementations must return a consistent value even when the tick
 * interrupt fires during the read (coding standard R15).
 *
 * @param out_ms  Receives the uptime in milliseconds.
 * @return GG_OK on success, GG_ERR_NULL if out_ms is NULL.
 * @req REQ-LOG-002
 */
// DEV-04: HAL interface or test control API, one definition on the PC
// cppcheck-suppress misra-c2012-8.7
gg_status_t hal_timer_get_ms(uint32_t *out_ms);

#endif /* HAL_TIMER_H */
